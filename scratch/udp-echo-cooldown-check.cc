// Deterministic regression: delayed echoes arrive after the send deadline.
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <iostream>
#include <vector>

using namespace ns3;
static std::vector<Time> sent, received;
static void Tx(Ptr<const Packet>) { sent.push_back(Simulator::Now()); }
static void Rx(Ptr<const Packet>, Address) { received.push_back(Simulator::Now()); }

static bool Check(bool deadline, bool lateStart)
{
  sent.clear(); received.clear();
  NodeContainer nodes; nodes.Create(2);
  InternetStackHelper internet; internet.Install(nodes);
  PointToPointHelper link;
  link.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  link.SetChannelAttribute("Delay", TimeValue(MilliSeconds(200)));
  NetDeviceContainer devices = link.Install(nodes);
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);
  UdpEchoServerHelper server(9);
  server.Install(nodes.Get(1)).Start(Seconds(0));
  UdpEchoClientHelper helper(interfaces.GetAddress(1), 9);
  helper.SetAttribute("MaxPackets", UintegerValue(100));
  helper.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
  helper.SetAttribute("PacketSize", UintegerValue(96));
  if (deadline) helper.SetAttribute("SendUntil", TimeValue(MilliSeconds(1300)));
  ApplicationContainer client = helper.Install(nodes.Get(0));
  client.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&Tx));
  client.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&Rx));
  client.Start(MilliSeconds(lateStart ? 1300 : 1000));
  client.Stop(MilliSeconds(deadline ? 2000 : 1300));
  Simulator::Stop(MilliSeconds(2100));
  Simulator::Run();
  bool ok = sent.size() == (lateStart ? 0u : 3u) &&
            received.size() == (deadline && !lateStart ? 3u : 0u);
  for (Time t : sent) ok = ok && t < MilliSeconds(1300);
  for (Time t : received) ok = ok && t > MilliSeconds(1300);
  std::cout << "deadline=" << deadline << " late_start=" << lateStart
            << " sent=" << sent.size() << " received=" << received.size()
            << " pass=" << ok << std::endl;
  Simulator::Destroy();
  return ok;
}

int main()
{
  bool legacy = Check(false, false);
  bool cooldown = Check(true, false);
  bool boundary = Check(true, true);
  return legacy && cooldown && boundary ? 0 : 1;
}
