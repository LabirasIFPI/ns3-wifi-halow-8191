// Four real STAs across the AID space; diagnostic, not a conformance test.
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/extension-headers.h"
#include <fstream>
#include <iostream>
#include <map>
#include <set>

using namespace ns3;
static const uint16_t aids[] = {1, 2048, 4096, 8191};
static uint32_t assocEvents[4] = {}, deassocEvents[4] = {};
static uint32_t sent[4] = {}, received[4] = {}, echoed[4] = {};
static std::set<uint32_t> associated;
static std::map<Mac48Address, uint32_t> stationIndex;
static std::map<Ipv4Address, uint32_t> ipIndex;
static std::ofstream transmissions, beacons;
static Time rawStart;
static uint8_t rawPage = 0;
static bool haveBeacon = false;
static uint32_t wrongSlot = 0, observedData = 0;
static int64_t lastStaBeaconUs[4] = {-1, -1, -1, -1};
static int lastStaPage[4] = {-1, -1, -1, -1};

static void StaRxEnd (uint32_t i, Ptr<const Packet> packet)
{
  Ptr<Packet> copy = packet->Copy ();
  WifiMacHeader header;
  copy->RemoveHeader (header);
  if (!header.IsS1gBeacon ()) return;
  S1gBeaconHeader beacon;
  copy->RemoveHeader (beacon);
  lastStaBeaconUs[i] = Simulator::Now ().GetMicroSeconds ();
  lastStaPage[i] = beacon.GetRPS ().GetRawAssigmentObj (0).GetRawGroupPage ();
}

static void Assoc (uint32_t i, Mac48Address)
{
  ++assocEvents[i];
  associated.insert (i);
  std::cout << "associated index=" << i << " time_s=" << Simulator::Now ().GetSeconds () << std::endl;
}
static void Deassoc (uint32_t i, Mac48Address) { ++deassocEvents[i]; associated.erase (i); }
static void Sent (uint32_t i, Ptr<const Packet>) { ++sent[i]; }
static void Echoed (uint32_t i, Ptr<const Packet>, Address) { ++echoed[i]; }
static void Received (Ptr<const Packet>, Address from)
{
  const auto item = ipIndex.find (InetSocketAddress::ConvertFrom (from).GetIpv4 ());
  if (item != ipIndex.end ()) ++received[item->second];
}
static void BeaconEnd (Ptr<const Packet> packet)
{
  Ptr<Packet> copy = packet->Copy ();
  WifiMacHeader header;
  copy->RemoveHeader (header);
  if (!header.IsS1gBeacon ()) return;
  S1gBeaconHeader beacon;
  copy->RemoveHeader (beacon);
  rawPage = beacon.GetRPS ().GetRawAssigmentObj (0).GetRawGroupPage ();
  rawStart = Simulator::Now ();
  haveBeacon = true;
  beacons << rawStart.GetMicroSeconds () << ',' << unsigned (rawPage) << ','
          << unsigned (beacon.GetTIM ().GetPageIndex ()) << ','
          << unsigned (beacon.GetpageSlice ().GetPageindex ()) << '\n';
}
static void TxBegin (bool fromAp, Ptr<const Packet> packet)
{
  WifiMacHeader header;
  packet->PeekHeader (header);
  if (!header.IsData () || !haveBeacon) return;
  auto found = stationIndex.find (fromAp ? header.GetAddr1 () : header.GetAddr2 ());
  if (found == stationIndex.end ()) return; // Exclude broadcast/multicast.
  const uint16_t aid = aids[found->second];
  const int64_t elapsed = (Simulator::Now () - rawStart).GetMicroSeconds ();
  const int64_t slot = (aid & 2047) % 3;
  const bool duringRaw = elapsed >= 0 && elapsed < 59100; // 3 * 19700 us.
  const bool inOwnSlot = duringRaw && rawPage == (aid >> 11) &&
                        elapsed >= slot * 19700 && elapsed < (slot + 1) * 19700;
  const char *classification = !duringRaw ? "outside_raw" : inOwnSlot ? "own_slot" : "other_slot_or_page";
  if (duringRaw && !inOwnSlot) ++wrongSlot;
  ++observedData;
  transmissions << Simulator::Now ().GetMicroSeconds () << ',' << (fromAp ? "AP" : "STA")
                << ',' << aid << ',' << unsigned (rawPage) << ',' << elapsed << ','
                << classification << ',' << header.GetSequenceNumber () << ',' << header.IsRetry ()
                << ',' << header.IsQosData () << ',' << lastStaBeaconUs[found->second]
                << ',' << lastStaPage[found->second] << '\n';
}

int main (int argc, char **argv)
{
  std::string prefix = "results/four-page";
  bool staticArp = true;
  bool recovery = false;
  bool sliced = false;
  double queueDelay = 0.5;
  uint32_t seed = 1;
  CommandLine cmd;
  cmd.AddValue ("prefix", "Output path prefix (parent directory must exist)", prefix);
  cmd.AddValue ("staticArp", "Prepopulate ARP as scratch/test does", staticArp);
  cmd.AddValue ("seed", "Random seed", seed);
  cmd.AddValue ("recovery", "Move STA0 away temporarily to test beacon-loss recovery", recovery);
  cmd.AddValue ("sliced", "Two 16-block TIM slices per page", sliced);
  cmd.AddValue ("queueDelay", "Queue lifetime in seconds (must cover page recurrence)", queueDelay);
  cmd.Parse (argc, argv);
  Config::SetDefault ("ns3::WifiMacQueue::MaxDelay", TimeValue (Seconds (queueDelay)));
  RngSeedManager::SetSeed (seed);
  transmissions.open ((prefix + "-tx.csv").c_str ());
  beacons.open ((prefix + "-beacons.csv").c_str ());
  NS_ABORT_MSG_IF (!transmissions || !beacons, "Cannot open output files");
  transmissions << "time_us,sender,aid,raw_page,offset_after_beacon_us,classification,mac_sequence,retry,qos,last_sta_beacon_us,last_sta_page\n";
  beacons << "beacon_tx_end_us,raw_page,tim_page,page_slice_page\n";
  NodeContainer stations, ap;
  stations.Create (4); ap.Create (1);
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (stations); mobility.Install (ap);
  for (uint32_t i = 0; i < 4; ++i)
    stations.Get (i)->GetObject<MobilityModel> ()->SetPosition (Vector (i + 1, 0, 0));
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
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (Ssid ("four-page")),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer devices = wifi.Install (phy, mac, stations);
  for (uint32_t i = 0; i < 4; ++i)
    {
      uint8_t bytes[] = {2, 0, 0, 0, uint8_t (aids[i] >> 8), uint8_t (aids[i])};
      Mac48Address address; address.CopyFrom (bytes);
      devices.Get (i)->SetAddress (address);
      stationIndex[address] = i;
      Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice> (devices.Get (i));
      device->GetMac ()->TraceConnectWithoutContext ("Assoc", MakeBoundCallback (&Assoc, i));
      device->GetMac ()->TraceConnectWithoutContext ("DeAssoc", MakeBoundCallback (&Deassoc, i));
      device->GetPhy ()->TraceConnectWithoutContext ("PhyTxBegin", MakeBoundCallback (&TxBegin, false));
      device->GetPhy ()->TraceConnectWithoutContext ("PhyRxEnd", MakeBoundCallback (&StaRxEnd, i));
    }
  RPSVector raw;
  for (uint8_t page = 0; page < 4; ++page)
    {
      RPS *rps = new RPS;
      RPS::RawAssignment group;
      group.SetRawControl (0); group.SetSlotCrossBoundary (1);
      group.SetSlotFormat (1); group.SetSlotDurationCount (160); group.SetSlotNum (3);
      group.SetRawGroup ((2047u << 13) | ((page == 0 ? 1u : 0u) << 2) | page);
      rps->SetRawAssignment (group); raw.rpsset.push_back (rps);
    }
  pageSlice slice;
  slice.SetPageindex (0); slice.SetPagePeriod (sliced ? 8 : 4); slice.SetPageSliceCount (sliced ? 2 : 0);
  slice.SetPageSliceLen (sliced ? 16 : 1); slice.SetBlockOffset (0); slice.SetTIMOffset (0);
  TIM tim; tim.SetPageIndex (0); tim.SetDTIMPeriod (sliced ? 2 : 1); tim.SetDTIMCount (0);
  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (Ssid ("four-page")),
      "NRawStations", UintegerValue (4), "RPSsetup", RPSVectorValue (raw),
      "PageSliceSet", pageSliceValue (slice), "TIMSet", TIMValue (tim),
      "RotateRawPages", BooleanValue (true));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, ap);
  Ptr<WifiNetDevice> apWifi = DynamicCast<WifiNetDevice> (apDevice.Get (0));
  apWifi->GetPhy ()->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&BeaconEnd));
  apWifi->GetPhy ()->TraceConnectWithoutContext ("PhyTxBegin", MakeBoundCallback (&TxBegin, true));
  InternetStackHelper internet; internet.Install (stations); internet.Install (ap);
  Ipv4AddressHelper addresses; addresses.SetBase ("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer staIp = addresses.Assign (devices);
  Ipv4InterfaceContainer apIp = addresses.Assign (apDevice);
  if (staticArp)
    {
      NetDeviceContainer allDevices; allDevices.Add (devices); allDevices.Add (apDevice);
      Ipv4InterfaceContainer allIp; allIp.Add (staIp); allIp.Add (apIp);
      for (uint32_t i = 0; i < allDevices.GetN (); ++i)
        {
          Ptr<ArpCache> cache = CreateObject<ArpCache> ();
          cache->SetAliveTimeout (Seconds (3600));
          for (uint32_t j = 0; j < allDevices.GetN (); ++j)
            {
              if (i == j) continue;
              ArpCache::Entry *entry = cache->Add (allIp.GetAddress (j));
              entry->MarkWaitReply (0);
              entry->MarkAlive (Mac48Address::ConvertFrom (allDevices.Get (j)->GetAddress ()));
            }
          Ptr<Ipv4L3Protocol> ip = allDevices.Get (i)->GetNode ()->GetObject<Ipv4L3Protocol> ();
          ip->GetInterface (ip->GetInterfaceForDevice (allDevices.Get (i)))->SetArpCache (cache);
        }
    }
  std::cout << "static_arp=" << staticArp << std::endl;
  UdpEchoServerHelper server (9);
  ApplicationContainer serverApps = server.Install (ap.Get (0));
  serverApps.Start (Seconds (0));
  serverApps.Get (0)->TraceConnectWithoutContext ("Rx", MakeCallback (&Received));
  for (uint32_t i = 0; i < 4; ++i)
    {
      ipIndex[staIp.GetAddress (i)] = i;
      UdpEchoClientHelper client (apIp.GetAddress (0), 9);
      client.SetAttribute ("MaxPackets", UintegerValue (10));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
      client.SetAttribute ("PacketSize", UintegerValue (96));
      ApplicationContainer app = client.Install (stations.Get (i));
      app.Start (Seconds ((recovery ? 8 : 3) + i * 0.05));
      app.Get (0)->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&Sent, i));
      app.Get (0)->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&Echoed, i));
    }
  if (recovery)
    {
      Ptr<MobilityModel> moving = stations.Get (0)->GetObject<MobilityModel> ();
      Simulator::Schedule (Seconds (2), &MobilityModel::SetPosition, moving, Vector (1000000, 0, 0));
      Simulator::Schedule (Seconds (5), &MobilityModel::SetPosition, moving, Vector (1, 0, 0));
    }
  Simulator::Stop (Seconds (recovery ? 20 : 15));
  Simulator::Run ();
  std::ofstream summary ((prefix + "-summary.csv").c_str ());
  bool passed = associated.size () == 4 && haveBeacon && observedData > 0 && wrongSlot == 0;
  summary << "aid,associated_at_end,requests_sent,server_receipts,client_echoes\n";
  for (uint32_t i = 0; i < 4; ++i)
    {
      Ptr<StaWifiMac> sta = DynamicCast<StaWifiMac> (DynamicCast<WifiNetDevice> (devices.Get (i))->GetMac ());
      std::cout << "aid=" << sta->GetAID () << " sent=" << sent[i]
                << " server_rx=" << received[i] << " echoed=" << echoed[i] << std::endl;
      summary << aids[i] << ',' << associated.count (i) << ',' << sent[i] << ',' << received[i] << ',' << echoed[i] << '\n';
      passed = passed && sta->GetAID () == aids[i] && sent[i] == 10 && received[i] == 10 && echoed[i] == 10;
    }
  std::cout << "associated_at_end=" << associated.size () << " data_tx_observed=" << observedData
            << " starts_in_other_raw_slot_or_page=" << wrongSlot << std::endl;
  if (recovery)
    {
      std::cout << "recovery assoc_events=" << assocEvents[0] << " deassoc_events=" << deassocEvents[0] << std::endl;
      passed = passed && assocEvents[0] >= 2 && deassocEvents[0] >= 1;
    }
  Simulator::Destroy ();
  for (auto rps : raw.rpsset) delete rps;
  std::cout << (passed ? "PASS" : "FAIL") << ": four-STA diagnostic; not an 8191-STA capacity test" << std::endl;
  return passed ? 0 : 2;
}
