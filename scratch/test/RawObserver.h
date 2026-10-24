// Observe serialized beacons and unicast data starts, using AP PHY time.
#ifndef TEST_RAW_OBSERVER_H
#define TEST_RAW_OBSERVER_H
#include "ns3/extension-headers.h"
namespace raw_observer {
struct Window { uint16_t first, last; uint8_t page, slots; int64_t start, duration; };
static std::vector<Window> windows;
static std::ofstream beaconCsv, txCsv;
static std::map<Mac48Address, uint16_t> aids;
static Time origin;
static uint8_t announcedPage = 0;
static uint64_t beacons = 0, data = 0, wrong = 0, mismatch = 0;
static void BeaconEnd(Ptr<const Packet> packet) {
    Ptr<Packet> copy = packet->Copy();
    WifiMacHeader header; copy->RemoveHeader(header);
    if (!header.IsS1gBeacon()) return;
    S1gBeaconHeader beacon; copy->RemoveHeader(beacon);
    if (beacon.GetTIM().GetDTIMCount() == 0) announcedPage = beacon.GetpageSlice().GetPageindex();
    RPS rps = beacon.GetRPS();
    windows.clear(); origin = Simulator::Now(); ++beacons;
    int64_t start = 0;
    for (uint32_t g = 0; g < rps.GetNumberOfRawGroups(); ++g) {
        auto raw = rps.GetRawAssigmentObj(g);
        Window w = {raw.GetRawGroupAIDStart(), raw.GetRawGroupAIDEnd(),
                    raw.GetRawGroupPage(), static_cast<uint8_t>(raw.GetSlotNum()), start,
                    500 + 120 * static_cast<int64_t>(raw.GetSlotDurationCount())};
        windows.push_back(w); start += w.duration * w.slots;
        if (w.page != beacon.GetTIM().GetPageIndex() || w.page != announcedPage) ++mismatch;
        beaconCsv << origin.GetMicroSeconds() << ',' << g << ',' << unsigned(w.page) << ','
                  << unsigned(beacon.GetTIM().GetPageIndex()) << ','
                  << unsigned(announcedPage) << ',' << unsigned(beacon.GetTIM().GetDTIMCount()) << ','
                  << unsigned(beacon.GetTIM().GetPageSliceNum()) << ',' << unsigned(beacon.GetTIM().m_length) << '\n';
    }
}
static void TxBegin(bool fromAp, Ptr<const Packet> packet) {
    WifiMacHeader header; packet->PeekHeader(header);
    if (!header.IsData() || header.GetAddr1().IsGroup() || !beacons) return;
    auto found = aids.find(fromAp ? header.GetAddr1() : header.GetAddr2());
    if (found == aids.end()) return;
    const uint16_t aid = found->second, local = aid & 2047;
    const int64_t elapsed = (Simulator::Now() - origin).GetMicroSeconds();
    bool during = false, own = false;
    for (const auto &w : windows) {
        if (elapsed >= w.start && elapsed < w.start + w.duration * w.slots) {
            during = true;
            const int64_t slotStart = w.start + (local % w.slots) * w.duration;
            own = w.page == (aid >> 11) && w.first <= local && local <= w.last &&
                  elapsed >= slotStart && elapsed < slotStart + w.duration;
        }
    }
    ++data;
    if (during && !own) ++wrong;
    txCsv << Simulator::Now().GetMicroSeconds() << ',' << (fromAp ? "AP" : "STA") << ',' << aid << ','
          << (during ? (own ? "own_slot" : "other_slot_or_page") : "outside_raw") << ','
          << header.GetSequenceNumber() << ',' << header.IsRetry() << '\n';
}
static void Install(NetDeviceContainer stations, NetDeviceContainer ap, const std::string &prefix) {
    beaconCsv.open((prefix + "-beacons.csv").c_str()); txCsv.open((prefix + "-tx.csv").c_str());
    NS_ABORT_MSG_IF(!beaconCsv || !txCsv, "Cannot open RAW observation files");
    beaconCsv << "beacon_end_us,group,raw_page,tim_page,page_slice_page,dtim_count,tim_slice,bitmap_bytes\n";
    txCsv << "time_us,sender,aid,classification,sequence,retry\n";
    for (uint32_t i = 0; i < stations.GetN(); ++i) {
        auto device = DynamicCast<WifiNetDevice>(stations.Get(i));
        aids[Mac48Address::ConvertFrom(device->GetAddress())] = i + 1;
        device->GetPhy()->TraceConnectWithoutContext("PhyTxBegin", MakeBoundCallback(&TxBegin, false));
    }
    auto device = DynamicCast<WifiNetDevice>(ap.Get(0));
    device->GetPhy()->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&BeaconEnd));
    device->GetPhy()->TraceConnectWithoutContext("PhyTxBegin", MakeBoundCallback(&TxBegin, true));
}
}
#endif
