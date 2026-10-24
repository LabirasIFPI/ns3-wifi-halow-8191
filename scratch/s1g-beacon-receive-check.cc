#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mac-rx-middle.h"
#include <iostream>
using namespace ns3;
static uint32_t forwarded = 0;
static void Forward (Ptr<Packet>, const WifiMacHeader*) { ++forwarded; }
int main ()
{
  Mac48Address ap ("02:00:00:00:20:00"), sta ("02:00:00:00:00:01");
  // Extension headers do not carry Retry or Sequence Control. Reusing a
  // header that held a retry must not import those fields into a beacon.
  WifiMacHeader beacon;
  beacon.SetS1gBeacon (); beacon.SetAddr1 (Mac48Address::GetBroadcast ());
  beacon.SetAddr2 (ap); beacon.SetAddr3 (ap); beacon.SetDuration (MicroSeconds (0));
  beacon.SetNoNextTBTT (); beacon.SetNoCompressedSSID (); beacon.SetNoANO ();
  beacon.SetNoSecurity (); beacon.SetNoAPPM (); beacon.SetBSSBW (S1G_BSS_BW_F1_T2);
  Ptr<Packet> wire = Create<Packet> (); wire->AddHeader (beacon);
  WifiMacHeader decoded;
  decoded.SetRetry (); decoded.SetSequenceNumber (123); decoded.SetFragmentNumber (0);
  wire->RemoveHeader (decoded);
  bool clean = !decoded.IsRetry () && decoded.GetSequenceControl () == 0;
  MacRxMiddle middle; middle.SetForwardCallback (MakeCallback (&Forward));
  WifiMacHeader unicast;
  unicast.SetAssocResp (); unicast.SetAddr1 (sta); unicast.SetAddr2 (ap);
  unicast.SetAddr3 (ap); unicast.SetNoRetry (); unicast.SetNoMoreFragments ();
  unicast.SetSequenceNumber (123); unicast.SetFragmentNumber (0);
  middle.Receive (Create<Packet> (10), &unicast);
  middle.Receive (Create<Packet> (10), &decoded);
  // An unfinished unicast fragment must not suppress independent beacons.
  unicast.SetSequenceNumber (124); unicast.SetMoreFragments ();
  middle.Receive (Create<Packet> (10), &unicast);
  middle.Receive (Create<Packet> (10), &decoded);
  bool pass = clean && forwarded == 3;
  std::cout << "clean_extension_fields=" << clean << " forwarded=" << forwarded
            << " expected=3 " << (pass ? "PASS" : "FAIL") << std::endl;
  return pass ? 0 : 2;
}
