// Optional, read-only diagnostics. HALOW_DIAG_MACS is a comma-separated MAC list.
#ifndef HALOW_ASSOCIATION_DIAGNOSTICS_H
#define HALOW_ASSOCIATION_DIAGNOSTICS_H
#include "ns3/mac48-address.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "wifi-mac-header.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <set>
namespace ns3 {
inline bool UplinkDiagnosticReceiver (Mac48Address address)
{
  static const char *value = std::getenv ("HALOW_DIAG_AP_MAC");
  static const Mac48Address ap = value ? Mac48Address(value) : Mac48Address();
  return value && address == ap;
}
inline bool AssociationDiagnosticSelected (Mac48Address address)
{
  static const std::set<Mac48Address> selected = [] () {
    std::set<Mac48Address> result;
    const char* value = std::getenv ("HALOW_DIAG_MACS");
    if (value) {
      std::istringstream input (value);
      std::string item;
      while (std::getline (input, item, ','))
        if (!item.empty ()) result.insert (Mac48Address (item.c_str ()));
    }
    return result;
  } ();
  return selected.find (address) != selected.end ();
}
}
#define HALOW_DIAG(address, event, fields) do { \
  if (ns3::AssociationDiagnosticSelected (address)) \
    std::clog << "ASSOC_DIAG t_ns=" << ns3::Simulator::Now ().GetNanoSeconds () \
      << " mac=" << address << " event=" << event << " " << fields << "\n"; \
} while (false)
// Inspect only frame-control byte: safe for short packets, no parsing/mutation.
#define HALOW_BEACON_DIAG(address, packet, event, fields) do { \
  if (ns3::AssociationDiagnosticSelected (address) && (packet)->GetSize () >= 1) { \
    uint8_t halowFrameControl = 0; (packet)->CopyData (&halowFrameControl, 1); \
    if ((halowFrameControl & 0xfc) == 0x1c) \
      HALOW_DIAG (address, event, "uid=" << (packet)->GetUid () << " " << fields); \
  } \
} while (false)
// Header is present at PHY; only inspect packets received by the selected AP.
#define HALOW_UPLINK_PHY(receiver, packet, event, fields) do { \
  if (ns3::UplinkDiagnosticReceiver(receiver) && (packet)->GetSize() >= 26) { \
    ns3::WifiMacHeader uplinkHeader; (packet)->PeekHeader(uplinkHeader); \
    if (uplinkHeader.IsData() && uplinkHeader.IsToDs() && !uplinkHeader.IsFromDs()) \
      HALOW_DIAG(uplinkHeader.GetAddr2(), event, "uid=" << (packet)->GetUid() \
        << " seq=" << uplinkHeader.GetSequenceNumber() << " retry=" << uplinkHeader.IsRetry() << " " << fields); \
  } \
} while (false)
#endif
