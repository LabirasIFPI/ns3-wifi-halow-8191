// Direct MAC regression, without a radio simulation. Exercise real request
// validation and response queueing, then inject ACK/failure callbacks.
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include <iostream>
using namespace ns3;
namespace ns3 {
class ApRepeatedAssociationCheck {
public:
  static void Request(Ptr<ApWifiMac> ap, Mac48Address sta) {
    MgtAssocRequestHeader request;
    request.SetSsid(ap->GetSsid());
    request.SetSupportedRates(ap->GetSupportedRates());
    Ptr<Packet> packet = Create<Packet>(); packet->AddHeader(request);
    WifiMacHeader header; header.SetAssocReq();
    header.SetAddr1(ap->GetAddress()); header.SetAddr2(sta);
    header.SetAddr3(ap->GetAddress());
    ap->Receive(packet, &header);
  }
  static bool Run() {
    NodeContainer nodes; nodes.Create(1);
    WifiHelper wifi = WifiHelper::Default();
    wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    mac.SetType("ns3::ApWifiMac", "BeaconGeneration", BooleanValue(false));
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);
    auto device = DynamicCast<WifiNetDevice>(devices.Get(0));
    auto ap = DynamicCast<ApWifiMac>(device->GetMac());
    auto manager = device->GetRemoteStationManager();
    Mac48Address sta("00:00:00:00:00:02");
    WifiMacHeader response; response.SetAssocResp(); response.SetAddr1(sta);
    Request(ap, sta);
    bool initial = manager->IsWaitAssocTxOk(sta) && !manager->IsAssociated(sta);
    ap->TxOk(response);
    bool ack = manager->IsAssociated(sta);
    Request(ap, sta);
    bool repeated = manager->IsAssociated(sta) && !manager->IsWaitAssocTxOk(sta);
    // Simulate expiration of the queued redundant response without ACK.
    ap->m_dca->GetQueue()->Flush();
    bool expired = manager->IsAssociated(sta);
    ap->TxFailed(response);
    bool lateFailure = manager->IsAssociated(sta);
    manager->RecordDisassociated(sta);
    Request(ap, sta);
    bool recovery = manager->IsWaitAssocTxOk(sta) && !manager->IsAssociated(sta);
    ap->TxFailed(response);
    bool failedNew = !manager->IsAssociated(sta) && !manager->IsWaitAssocTxOk(sta);
    Request(ap, sta); ap->TxOk(response);
    bool recovered = manager->IsAssociated(sta);
    bool pass = initial && ack && repeated && expired && lateFailure && recovery && failedNew && recovered;
    std::cout << "initial_wait=" << initial << " ack=" << ack
      << " repeated_preserved=" << repeated << " expired_preserved=" << expired
      << " failed_repeat_preserved=" << lateFailure << " recovery_wait=" << recovery
      << " new_failure_rejected=" << failedNew << " recovered=" << recovered
      << " pass=" << pass << std::endl;
    Simulator::Destroy();
    return pass;
  }
};
}
int main() { return ApRepeatedAssociationCheck::Run() ? 0 : 1; }
