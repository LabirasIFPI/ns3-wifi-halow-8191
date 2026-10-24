/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 MIRKO BANCHI
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "s1g-test-tim-raw.h"
#include <set>
#include <memory>
#include <stdexcept>
#include <cmath>

#include "RawObserver.h"

NS_LOG_COMPONENT_DEFINE("s1g-wifi-network-tim-raw");

bool trafficStarted = false;
EventId associationDeadline;
uint32_t AssocNum = 0;
int64_t AssocTime = 0;
uint32_t StaNum = 0;
NetDeviceContainer staDeviceCont;
// Per-STA statistics capacity; the AP is not included in these arrays.
// Multi-page RAW/TIM operation must be validated separately.
const int MaxSta = 8191;

Configuration config;
Statistics stats;
SimulationEventManager eventManager;

// This scenario has one Wi-Fi device (index 0) per node. Resolve its
// trace directly, preserving the full context string expected by callbacks.
// Config::Connect repeatedly scans the global NodeList for each station.
void ConnectNodeTrace(const std::string &path, const CallbackBase &callback) {
    const uint32_t index = std::stoul(path.substr(std::string("/NodeList/").size()));
    Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice>(NodeList::GetNode(index)->GetDevice(0));
    Ptr<Object> target;
    if (path.find("/Mac/") != std::string::npos) target = device->GetMac();
    else if (path.find("/RemoteStationManager/") != std::string::npos) target = device->GetRemoteStationManager();
    else if (path.find("/State/State") != std::string::npos) {
        PointerValue state;
        device->GetPhy()->GetAttribute("State", state);
        target = state.GetObject();
    } else target = device->GetPhy();
    const std::string trace = path.substr(path.find_last_of('/') + 1);
    NS_ABORT_MSG_IF(!target || !target->TraceConnect(trace, path, callback),
                    "Unable to connect trace " << path);
}

class assoc_record {
public:
	assoc_record();
	bool GetAssoc();
	void SetAssoc(std::string context, Mac48Address address);
	void UnsetAssoc(std::string context, Mac48Address address);
	void setstaid(uint16_t id);
private:
	bool assoc;
	uint16_t staid;
};

assoc_record::assoc_record() {
	assoc = false;
	staid = 65535;
}

void assoc_record::setstaid(uint16_t id) {
	staid = id;
}

void assoc_record::SetAssoc(std::string context, Mac48Address address) {
	assoc = true;
}

void assoc_record::UnsetAssoc(std::string context, Mac48Address address) {
	assoc = false;
}

bool assoc_record::GetAssoc() {
	return assoc;
}

typedef std::vector<assoc_record *> assoc_recordVector;
assoc_recordVector assoc_vector;

uint32_t GetAssocNum() {
	AssocNum = 0;
	for (assoc_recordVector::const_iterator index = assoc_vector.begin();
			index != assoc_vector.end(); index++) {
		if ((*index)->GetAssoc()) {
			AssocNum++;
		}
	}
	return AssocNum;
}

void PopulateArpCache() {
	Ptr<ArpCache> arp = CreateObject<ArpCache>();
	arp->SetAliveTimeout(Seconds(3600 * 24 * 365));
	for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {
		Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol>();
		NS_ASSERT(ip != 0);
		ObjectVectorValue interfaces;
		ip->GetAttribute("InterfaceList", interfaces);
		for (ObjectVectorValue::Iterator j = interfaces.Begin();
				j != interfaces.End(); j++) {
			Ptr<Ipv4Interface> ipIface =
					(j->second)->GetObject<Ipv4Interface>();
			NS_ASSERT(ipIface != 0);
			Ptr<NetDevice> device = ipIface->GetDevice();
			NS_ASSERT(device != 0);
			Mac48Address addr = Mac48Address::ConvertFrom(device->GetAddress());
			for (uint32_t k = 0; k < ipIface->GetNAddresses(); k++) {
				Ipv4Address ipAddr = ipIface->GetAddress(k).GetLocal();
				if (ipAddr == Ipv4Address::GetLoopback())
					continue;
				ArpCache::Entry * entry = arp->Add(ipAddr);
				entry->MarkWaitReply(0);
				entry->MarkAlive(addr);
				std::cout << "Arp Cache: Adding the pair (" << addr << ","
						<< ipAddr << ")" << std::endl;
			}
		}
	}
	for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {
		Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol>();
		NS_ASSERT(ip != 0);
		ObjectVectorValue interfaces;
		ip->GetAttribute("InterfaceList", interfaces);
		for (ObjectVectorValue::Iterator j = interfaces.Begin();
				j != interfaces.End(); j++) {
			Ptr<Ipv4Interface> ipIface =
					(j->second)->GetObject<Ipv4Interface>();
			ipIface->SetAttribute("ArpCache", PointerValue(arp));
		}
	}
}

uint16_t ngroup;
uint16_t nslot;
RPSVector configureRAW(RPSVector rpslist, string RAWConfigFile) {
    // Validate before packing: setters would otherwise truncate invalid fields.
    ifstream input(RAWConfigFile);
    if (!input) {
        throw std::runtime_error("Unable to open RAW file: " + RAWConfigFile);
    }
    auto read = [&input]() -> uint32_t {
        int64_t value;
        if (!(input >> value) || value < 0 || value > 65535) {
            throw std::runtime_error("Missing, malformed or out-of-range RAW field");
        }
        return static_cast<uint32_t>(value);
    };
    const uint32_t nrps = read();
    if (nrps == 0) {
        throw std::runtime_error("RAW requires at least one RPS");
    }
    std::set<uint32_t> population;
    std::vector<std::unique_ptr<RPS> > parsed;
    for (uint32_t r = 0; r < nrps; ++r) {
        const uint32_t groups = read();
        if (groups == 0) {
            throw std::runtime_error("RPS requires at least one RAW group");
        }
        std::unique_ptr<RPS> rps(new RPS);
        std::set<uint32_t> inRps;
        ngroup = groups;
        for (uint32_t g = 0; g < groups; ++g) {
            const uint32_t control = read(), csb = read(), format = read();
            const uint32_t duration = read(), slots = read(), page = read();
            const uint32_t first = read(), last = read();
            if (control > 255 || csb > 1 || format > 1 || page > 3 ||
                first > last || last > 2047 || (page == 0 && first == 0) ||
                duration > (format ? 2047u : 255u) ||
                slots == 0 || slots > (format ? 7u : 63u)) {
                throw std::runtime_error("Invalid RAW fields at RPS " +
                    std::to_string(r + 1) + ", group " + std::to_string(g + 1));
            }
            for (uint32_t local = first; local <= last; ++local) {
                const uint32_t aid = (page << 11) | local;
                if (!inRps.insert(aid).second) {
                    throw std::runtime_error("Overlapping RAW groups in RPS " +
                        std::to_string(r + 1) + " at AID " + std::to_string(aid));
                }
                population.insert(aid);
            }
            RPS::RawAssignment raw;
            raw.SetRawControl(control);
            raw.SetSlotCrossBoundary(csb);
            raw.SetSlotFormat(format);
            raw.SetSlotDurationCount(duration);
            raw.SetSlotNum(slots);
            raw.SetRawGroup((last << 13) | (first << 2) | page);
            nslot = slots;
            rps->SetRawAssignment(raw);
        }
        parsed.push_back(std::move(rps));
    }
    input >> std::ws;
    if (!input.eof()) {
        throw std::runtime_error("Unexpected trailing RAW fields");
    }
    const uint32_t stations = config.Nsta == 0 ? population.size() : config.Nsta;
    if (stations == 0 || stations > MaxSta || population.size() != stations) {
        throw std::runtime_error("RAW unique population " + std::to_string(population.size()) +
            " does not match supported Nsta " + std::to_string(stations) + " (1..8191)");
    }
    // This scenario creates consecutive station MACs/AIDs starting at one.
    for (uint32_t aid = 1; aid <= stations; ++aid) {
        if (!population.count(aid)) {
            throw std::runtime_error("RAW does not cover scenario AID " + std::to_string(aid));
        }
    }
    if (config.NRawSta != -1 && config.NRawSta != static_cast<int>(population.size())) {
        throw std::runtime_error("NRawSta disagrees with unique RAW population");
    }
    config.Nsta = stations;
    config.NRawSta = population.size();
    for (auto &rps : parsed) {
        rpslist.rpsset.push_back(rps.release());
    }
    std::cout << "RAW population validated: " << stations << " unique AIDs, "
              << nrps << " RPS; coverage 1.." << stations << std::endl;
    return rpslist;
}

/*
pageslice element and TIM(DTIM) together accomplish page slicing.

Prior knowledge:
802.11ah support up to 8192 stations, they are constructed into: page, block,
 subblock, sta.
there are 13 bit represent the AID of stations.
 AID[11-12] represent page.
 AID[6-10] represent block.
 AID[3-5] represent subblock.
 AID[0-2] represent sta.

A TIM(DTIM) element only support one page
A Page slice element only support one page

 Concept of page slicing:
 Between two DTIM beacon, there are many TIM beacons, only allow a TIM beacon include some blocks of one page is called page slice. One TIM beacon is called a page slice.
 Page slcie element specify number of page slice between two DTIM, number of blocks in each
 page slice.
 Page slice element only appears together with DTIM.

 Details:
 Page slice element also indicates AP has buffered data for which block, if a station is in that block, the station should first sleep, then wake up at coresponding page slice(TIM beacon) which includes that block.

 When station wake up at that block, it check whether AP has data for itself. If has, keep awake to receive packets and go to sleep in the next beacon.
 */

void configurePageSlice (void)
{
    config.pageS.SetPageindex (config.pageIndex);
    config.pageS.SetPagePeriod (config.pagePeriod); //2 TIM groups between DTIMs
    config.pageS.SetPageSliceLen (config.pageSliceLength); //each TIM group has 1 block (2 blocks in 2 TIM groups)
    config.pageS.SetPageSliceCount (config.pageSliceCount);
    config.pageS.SetBlockOffset (config.blockOffset);
    config.pageS.SetTIMOffset (config.timOffset);
    //std::cout << "pageIndex=" << (int)config.pageIndex << ", pagePeriod=" << (int)config.pagePeriod << ", pageSliceLength=" << (int)config.pageSliceLength << ", pageSliceCount=" << (int)config.pageSliceCount << ", blockOffset=" << (int)config.blockOffset << ", timOffset=" << (int)config.timOffset << std::endl;
    // page 0
    // 8 TIM(page slice) for one page
    // 4 block (each page)
    // 8 page slice
    // both offset are 0
}

void configureTIM (void)
{
    config.tim.SetPageIndex (config.pageIndex);
    if (config.pageSliceCount)
    	config.tim.SetDTIMPeriod (config.pageSliceCount); // not necessarily the same
    else
    	config.tim.SetDTIMPeriod (1);

    //std::cout << "DTIM period=" << (int)config.pagePeriod << std::endl;
}

void checkRawAndTimConfiguration (void)
{
	std::cout << "Checking RAW and TIM configuration..." << std::endl;
	bool configIsCorrect = true;
	NS_ASSERT (config.rps.rpsset.size());
	// Number of page slices in a single page has to equal number of different RPS elements because
	// If #PS > #RPS, the same RPS will be used in more than 1 PS and that is wrong because
	// each PS can accommodate different AIDs (same RPS means same stations in RAWs)
    if(config.pageSliceCount)
    {
	//NS_ASSERT (config.pagePeriod == config.rps.rpsset.size());
    }
	for (uint32_t j = 0; j < config.rps.rpsset.size(); j++)
	{
		uint32_t totalRawTime = 0;
		for (uint32_t i = 0; i < config.rps.rpsset[j]->GetNumberOfRawGroups(); i++)
		{
			totalRawTime += (120 * config.rps.rpsset[j]->GetRawAssigmentObj(i).GetSlotDurationCount() + 500) * config.rps.rpsset[j]->GetRawAssigmentObj(i).GetSlotNum();
			auto aidStart = config.rps.rpsset[j]->GetRawAssigmentObj(i).GetRawGroupAIDStart();
			auto aidEnd = config.rps.rpsset[j]->GetRawAssigmentObj(i).GetRawGroupAIDEnd();
			configIsCorrect = check (aidStart, j) && check (aidEnd, j);
			// AIDs in each RPS must comply with TIM in the following way:
			// TIM0: 1-63; TIM1: 64-127; TIM2: 128-191; ...; TIM32: 1983-2047
			// If RPS that belongs to TIM0 includes other AIDs (other than range [1-63]) configuration is incorrect
			NS_ASSERT (configIsCorrect);
		}
		NS_ASSERT (totalRawTime <= config.BeaconInterval);
	}
}
// assumes each TIM has its own beacon - doesn't need to be the case as there has to be only PageSliceCount beacons between DTIMs
bool check (uint16_t aid, uint32_t index)
{
	uint8_t block = (aid >> 6 ) & 0x001f;
	NS_ASSERT (config.pageS.GetPageSliceLen() > 0);
	//uint8_t toTim = (block - config.pageS.GetBlockOffset()) % config.pageS.GetPageSliceLen();
	if (index == config.pageS.GetPageSliceCount() - 1 && config.pageS.GetPageSliceCount() != 0)
	{
		// the last page slice has 32 - the rest blocks
		return (block <= 31) && (block >= index * config.pageS.GetPageSliceLen());
	}
	else if (config.pageS.GetPageSliceCount() == 0)
		return true;

	return (block >= index * config.pageS.GetPageSliceLen()) && (block < (index + 1) * config.pageS.GetPageSliceLen());
}


void sendStatistics(bool schedule) {
	eventManager.onUpdateStatistics(stats);
	eventManager.onUpdateSlotStatistics(
			transmissionsPerTIMGroupAndSlotFromAPSinceLastInterval,
			transmissionsPerTIMGroupAndSlotFromSTASinceLastInterval);
	// reset
	std::fill(transmissionsPerTIMGroupAndSlotFromAPSinceLastInterval.begin(),
			transmissionsPerTIMGroupAndSlotFromAPSinceLastInterval.end(), 0);
	std::fill(transmissionsPerTIMGroupAndSlotFromSTASinceLastInterval.begin(),
			transmissionsPerTIMGroupAndSlotFromSTASinceLastInterval.end(), 0);

	if (schedule)
		Simulator::Schedule(Seconds(config.visualizerSamplingInterval),	&sendStatistics, true);
}

void onSTADeassociated(int i) {
	eventManager.onNodeDeassociated(*nodes[i]);
}

void updateNodesQueueLength() {
	for (uint32_t i = 0; i < config.Nsta; i++) {
		nodes[i]->UpdateQueueLength();
		stats.get(i).EDCAQueueLength = nodes[i]->queueLength;
	}
	Simulator::Schedule(Seconds(0.5), &updateNodesQueueLength);
}

void onSTAAssociated(int i) {
	cout << "Node " << std::to_string(i) << " is associated and has aid "
			<< nodes[i]->aId << endl;

    for (uint32_t k = 0; k < config.rps.rpsset.size(); ++k) {
        for (uint32_t j = 0; j < config.rps.rpsset[k]->GetNumberOfRawGroups(); ++j) {
            const auto group = config.rps.rpsset[k]->GetRawAssigmentObj(j);
            const uint16_t local = nodes[i]->aId & 0x07ff;
            if (group.GetRawGroupPage() == (nodes[i]->aId >> 11) &&
                group.GetRawGroupAIDStart() <= local && local <= group.GetRawGroupAIDEnd()) {
                nodes[i]->rpsIndex = k + 1;
                nodes[i]->rawGroupNumber = j + 1;
                nodes[i]->rawSlotIndex = local % group.GetSlotNum() + 1;
            }
        }
    }

	eventManager.onNodeAssociated(*nodes[i]);

	// RPS, Raw group and RAW slot assignment

	if (!trafficStarted && GetAssocNum() == config.Nsta) {
        trafficStarted = true;
        Simulator::Cancel(associationDeadline);
		cout << "All " << AssocNum << " stations associated at " << Simulator::Now ().GetMicroSeconds () <<", configuring clients & server" << endl;

		// association complete, start sending packets
		stats.TimeWhenEverySTAIsAssociated = Simulator::Now();

		if (config.trafficType == "udp") {
			std::cout << "UDP" << std::endl;
			configureUDPServer();
			configureUDPClients();
		} else if (config.trafficType == "udpecho") {
			configureUDPEchoServer();
			configureUDPEchoClients();
		} else if (config.trafficType == "tcpecho") {
			configureTCPEchoServer();
			configureTCPEchoClients();
		} else if (config.trafficType == "tcppingpong") {
			configureTCPPingPongServer();
			configureTCPPingPongClients();
		} else if (config.trafficType == "tcpipcamera") {
			configureTCPIPCameraServer();
			configureTCPIPCameraClients();
		} else if (config.trafficType == "tcpfirmware") {
			configureTCPFirmwareServer();
			configureTCPFirmwareClients();
		} else if (config.trafficType == "tcpsensor") {
			configureTCPSensorServer();
			configureTCPSensorClients();
		}
        // Apps are installed during this callback; this ns-3 initializes their
        // start/stop delays relative to installation time.
        for (uint32_t n = 0; n < wifiStaNode.GetN(); ++n) {
            Ptr<Node> node = wifiStaNode.Get(n);
            for (uint32_t a = 0; a < node->GetNApplications(); ++a) {
                Ptr<Application> app = node->GetApplication(a);
                Ptr<UdpEchoClient> echo = DynamicCast<UdpEchoClient>(app);
                if (echo)
                  {
                    echo->SetAttribute("SendUntil", TimeValue(Simulator::Now() + Seconds(config.simulationTime)));
                    app->SetStopTime(Seconds(config.simulationTime + config.CoolDownPeriod));
                  }
                else
                  {
                    app->SetStopTime(Seconds(config.simulationTime));
                  }
            }
        }
        Simulator::Stop(Seconds(config.simulationTime + config.CoolDownPeriod));
        std::cout << "Traffic window: " << Simulator::Now().GetSeconds() << ".."
                  << Simulator::Now().GetSeconds() + config.simulationTime
                  << " s; cooldown=" << config.CoolDownPeriod << " s" << std::endl;
		updateNodesQueueLength();
	}
}

void RpsIndexTrace(uint16_t oldValue, uint16_t newValue) {
	currentRps = newValue;
	//cout << "RPS: " << newValue << " at " << Simulator::Now().GetMicroSeconds() << endl;
}

void RawGroupTrace(uint8_t oldValue, uint8_t newValue) {
	currentRawGroup = newValue;
	//cout << "	group " << std::to_string(newValue) << " at " << Simulator::Now().GetMicroSeconds() << endl;
}

void RawSlotTrace(uint8_t oldValue, uint8_t newValue) {
	currentRawSlot = newValue;
	//cout << "		slot " << std::to_string(newValue) << " at " << Simulator::Now().GetMicroSeconds() << endl;
}

void configureNodes(NodeContainer& wifiStaNode, NetDeviceContainer& staDevice) {
	cout << "Configuring STA Node trace sources..." << endl;

	for (uint32_t i = 0; i < config.Nsta; i++) {

		cout << "Hooking up trace sources for STA " << i << endl;

		NodeEntry* n = new NodeEntry(i, &stats, wifiStaNode.Get(i),
				staDevice.Get(i));

		n->SetAssociatedCallback([ = ] {onSTAAssociated(i);});
		n->SetDeassociatedCallback([ = ] {onSTADeassociated(i);});

		nodes.push_back(n);
		// hook up Associated and Deassociated events
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Assoc",
				MakeCallback(&NodeEntry::SetAssociation, n));
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/DeAssoc",
				MakeCallback(&NodeEntry::UnsetAssociation, n));
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/NrOfTransmissionsDuringRAWSlot",
				MakeCallback(
						&NodeEntry::OnNrOfTransmissionsDuringRAWSlotChanged,
						n));	//not implem

		//ConnectNodeTrace("/NodeList/" + std::to_string(i) + "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/S1gBeaconMissed", MakeCallback(&NodeEntry::OnS1gBeaconMissed, n));

		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/PacketDropped",
				MakeCallback(&NodeEntry::OnMacPacketDropped, n));
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Collision",
				MakeCallback(&NodeEntry::OnCollision, n));
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/TransmissionWillCrossRAWBoundary",
				MakeCallback(&NodeEntry::OnTransmissionWillCrossRAWBoundary,
						n)); //?

		// hook up TX
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyTxBegin",
				MakeCallback(&NodeEntry::OnPhyTxBegin, n));
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyTxEnd",
				MakeCallback(&NodeEntry::OnPhyTxEnd, n));
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyTxDropWithReason",
				MakeCallback(&NodeEntry::OnPhyTxDrop, n)); //?

		// hook up RX
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyRxBegin",
				MakeCallback(&NodeEntry::OnPhyRxBegin, n));
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyRxEnd",
				MakeCallback(&NodeEntry::OnPhyRxEnd, n));
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyRxDropWithReason",
				MakeCallback(&NodeEntry::OnPhyRxDrop, n));

		// hook up MAC traces
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/MacTxRtsFailed",
				MakeCallback(&NodeEntry::OnMacTxRtsFailed, n)); //?
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/MacTxDataFailed",
				MakeCallback(&NodeEntry::OnMacTxDataFailed, n));
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/MacTxFinalRtsFailed",
				MakeCallback(&NodeEntry::OnMacTxFinalRtsFailed, n)); //?
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/MacTxFinalDataFailed",
				MakeCallback(&NodeEntry::OnMacTxFinalDataFailed, n)); //?

		// hook up PHY State change
		ConnectNodeTrace(
				"/NodeList/" + std::to_string(i)
						+ "/DeviceList/0/$ns3::WifiNetDevice/Phy/State/State",
				MakeCallback(&NodeEntry::OnPhyStateChange, n));

	}
}

int getBandwidth(string dataMode) {
	if (dataMode == "MCS1_0" || dataMode == "MCS1_1" || dataMode == "MCS1_2"
			|| dataMode == "MCS1_3" || dataMode == "MCS1_4"
			|| dataMode == "MCS1_5" || dataMode == "MCS1_6"
			|| dataMode == "MCS1_7" || dataMode == "MCS1_8"
			|| dataMode == "MCS1_9" || dataMode == "MCS1_10")
		return 1;

	else if (dataMode == "MCS2_0" || dataMode == "MCS2_1"
			|| dataMode == "MCS2_2" || dataMode == "MCS2_3"
			|| dataMode == "MCS2_4" || dataMode == "MCS2_5"
			|| dataMode == "MCS2_6" || dataMode == "MCS2_7"
			|| dataMode == "MCS2_8")
		return 2;

	return 0;
}

string getWifiMode(string dataMode) {
	if (dataMode == "MCS1_0")
		return "OfdmRate300KbpsBW1MHz";
	else if (dataMode == "MCS1_1")
		return "OfdmRate600KbpsBW1MHz";
	else if (dataMode == "MCS1_2")
		return "OfdmRate900KbpsBW1MHz";
	else if (dataMode == "MCS1_3")
		return "OfdmRate1_2MbpsBW1MHz";
	else if (dataMode == "MCS1_4")
		return "OfdmRate1_8MbpsBW1MHz";
	else if (dataMode == "MCS1_5")
		return "OfdmRate2_4MbpsBW1MHz";
	else if (dataMode == "MCS1_6")
		return "OfdmRate2_7MbpsBW1MHz";
	else if (dataMode == "MCS1_7")
		return "OfdmRate3MbpsBW1MHz";
	else if (dataMode == "MCS1_8")
		return "OfdmRate3_6MbpsBW1MHz";
	else if (dataMode == "MCS1_9")
		return "OfdmRate4MbpsBW1MHz";
	else if (dataMode == "MCS1_10")
		return "OfdmRate150KbpsBW1MHz";

	else if (dataMode == "MCS2_0")
		return "OfdmRate650KbpsBW2MHz";
	else if (dataMode == "MCS2_1")
		return "OfdmRate1_3MbpsBW2MHz";
	else if (dataMode == "MCS2_2")
		return "OfdmRate1_95MbpsBW2MHz";
	else if (dataMode == "MCS2_3")
		return "OfdmRate2_6MbpsBW2MHz";
	else if (dataMode == "MCS2_4")
		return "OfdmRate3_9MbpsBW2MHz";
	else if (dataMode == "MCS2_5")
		return "OfdmRate5_2MbpsBW2MHz";
	else if (dataMode == "MCS2_6")
		return "OfdmRate5_85MbpsBW2MHz";
	else if (dataMode == "MCS2_7")
		return "OfdmRate6_5MbpsBW2MHz";
	else if (dataMode == "MCS2_8")
		return "OfdmRate7_8MbpsBW2MHz";
	return "";
}

void OnAPPhyRxDrop(std::string context, Ptr<const Packet> packet,
		DropReason reason) {
	// THIS REQUIRES PACKET METADATA ENABLE!
	auto pCopy = packet->Copy();
	auto it = pCopy->BeginItem();
	while (it.HasNext()) {

		auto item = it.Next();
		Callback<ObjectBase *> constructor = item.tid.GetConstructor();

		ObjectBase *instance = constructor();
		Chunk *chunk = dynamic_cast<Chunk *>(instance);
		chunk->Deserialize(item.current);

		if (dynamic_cast<WifiMacHeader*>(chunk)) {
			WifiMacHeader* hdr = (WifiMacHeader*) chunk;

			int staId = -1;
			if (!config.useV6) {
				for (uint32_t i = 0; i < staNodeInterface.GetN(); i++) {
					if (wifiStaNode.Get(i)->GetDevice(0)->GetAddress()
							== hdr->GetAddr2()) {
						staId = i;
						break;
					}
				}
			} else {
				for (uint32_t i = 0; i < staNodeInterface6.GetN(); i++) {
					if (wifiStaNode.Get(i)->GetDevice(0)->GetAddress()
							== hdr->GetAddr2()) {
						staId = i;
						break;
					}
				}
			}
			if (staId != -1) {
				stats.get(staId).NumberOfDropsByReasonAtAP[reason]++;
			}
			delete chunk;
			break;
		} else
			delete chunk;
	}

}

void OnAPPacketToTransmitReceived(string context, Ptr<const Packet> packet,
		Mac48Address to, bool isScheduled, bool isDuringSlotOfSTA,
		Time timeLeftInSlot) {
	int staId = -1;
	if (!config.useV6) {
		for (uint32_t i = 0; i < staNodeInterface.GetN(); i++) {
			if (wifiStaNode.Get(i)->GetDevice(0)->GetAddress() == to) {
				staId = i;
				break;
			}
		}
	} else {
		for (uint32_t i = 0; i < staNodeInterface6.GetN(); i++) {
			if (wifiStaNode.Get(i)->GetDevice(0)->GetAddress() == to) {
				staId = i;
				break;
			}
		}
	}
	if (staId != -1) {
		if (isScheduled)
			stats.get(staId).NumberOfAPScheduledPacketForNodeInNextSlot++;
		else {
			stats.get(staId).NumberOfAPSentPacketForNodeImmediately++;
			stats.get(staId).APTotalTimeRemainingWhenSendingPacketInSameSlot +=
					timeLeftInSlot;
		}
	}
}

void onChannelTransmission(Ptr<NetDevice> senderDevice, Ptr<Packet> packet) {
	int rpsIndex = currentRps - 1;
	int rawGroup = currentRawGroup - 1;
	int slotIndex = currentRawSlot - 1;
	//cout << rpsIndex << "		" << rawGroup << "		" << slotIndex << "		" << endl;

	uint64_t iSlot = slotIndex;
	if (rpsIndex > 0)
		for (int r = rpsIndex - 1; r >= 0; r--)
			for (int g = 0; g < config.rps.rpsset[r]->GetNumberOfRawGroups(); g++)
				iSlot += config.rps.rpsset[r]->GetRawAssigmentObj(g).GetSlotNum();

	if (rawGroup > 0)
		for (int i = rawGroup - 1; i >= 0; i--)
			iSlot += config.rps.rpsset[rpsIndex]->GetRawAssigmentObj(i).GetSlotNum();

	if (rpsIndex >= 0 && rawGroup >= 0 && slotIndex >= 0)
	{
		if (senderDevice->GetAddress() == apDevice.Get(0)->GetAddress())
		{
			// from AP
			transmissionsPerTIMGroupAndSlotFromAPSinceLastInterval[iSlot] += packet->GetSerializedSize();
		}
		else
		{
			// from STA
			transmissionsPerTIMGroupAndSlotFromSTASinceLastInterval[iSlot] += packet->GetSerializedSize();

		}
	}
	//std::cout << "------------- packetSerializedSize = " << packet->GetSerializedSize() << std::endl;
	//std::cout << "------------- txAP[" << iSlot <<"] = " << transmissionsPerTIMGroupAndSlotFromAPSinceLastInterval[iSlot] << std::endl;
	//std::cout << "------------- txSTA[" << iSlot <<"] = " << transmissionsPerTIMGroupAndSlotFromSTASinceLastInterval[iSlot] << std::endl;

}

int getSTAIdFromAddress(Ipv4Address from) {
	int staId = -1;
	for (int i = 0; i < staNodeInterface.GetN(); i++) {
		if (staNodeInterface.GetAddress(i) == from) {
			staId = i;
			break;
		}
	}
	return staId;
}

void udpPacketReceivedAtServer(Ptr<const Packet> packet, Address from) { //works
	//cout << "+++++++++++udpPacketReceivedAtServer" << endl;
	int staId = getSTAIdFromAddress(
			InetSocketAddress::ConvertFrom(from).GetIpv4());
	if (staId != -1)
		nodes[staId]->OnUdpPacketReceivedAtAP(packet);
	else
		cout << "*** Node could not be determined from received packet at AP "
				<< endl;
}

void tcpPacketReceivedAtServer(Ptr<const Packet> packet, Address from) {
	int staId = getSTAIdFromAddress(
			InetSocketAddress::ConvertFrom(from).GetIpv4());
	if (staId != -1)
		nodes[staId]->OnTcpPacketReceivedAtAP(packet);
	else
		cout << "*** Node could not be determined from received packet at AP "
				<< endl;
}

void tcpRetransmissionAtServer(Address to) {
	int staId = getSTAIdFromAddress(Ipv4Address::ConvertFrom(to));
	if (staId != -1)
		nodes[staId]->OnTcpRetransmissionAtAP();
	else
		cout << "*** Node could not be determined from received packet at AP "
				<< endl;
}

void tcpPacketDroppedAtServer(Address to, Ptr<Packet> packet,
		DropReason reason) {
	int staId = getSTAIdFromAddress(Ipv4Address::ConvertFrom(to));
	if (staId != -1) {
		stats.get(staId).NumberOfDropsByReasonAtAP[reason]++;
	}
}

void tcpStateChangeAtServer(TcpSocket::TcpStates_t oldState,
		TcpSocket::TcpStates_t newState, Address to) {

	int staId = getSTAIdFromAddress(
			InetSocketAddress::ConvertFrom(to).GetIpv4());
	if (staId != -1)
		nodes[staId]->OnTcpStateChangedAtAP(oldState, newState);
	else
		cout << "*** Node could not be determined from received packet at AP "
				<< endl;

	//cout << Simulator::Now().GetMicroSeconds() << " ********** TCP SERVER SOCKET STATE CHANGED FROM " << oldState << " TO " << newState << endl;
}

void tcpIPCameraDataReceivedAtServer(Address from, uint16_t nrOfBytes) {
	int staId = getSTAIdFromAddress(
			InetSocketAddress::ConvertFrom(from).GetIpv4());
	if (staId != -1)
		nodes[staId]->OnTcpIPCameraDataReceivedAtAP(nrOfBytes);
	else
		cout << "*** Node could not be determined from received packet at AP "
				<< endl;
}

void configureUDPServer() {
	UdpServerHelper myServer(9);
	serverApp = myServer.Install(wifiApNode);
	serverApp.Get(0)->TraceConnectWithoutContext("Rx",
			MakeCallback(&udpPacketReceivedAtServer));
	serverApp.Start(Seconds(0));

}

void configureUDPEchoServer() {
	UdpEchoServerHelper myServer(9);
	serverApp = myServer.Install(wifiApNode);
	serverApp.Get(0)->TraceConnectWithoutContext("Rx",
			MakeCallback(&udpPacketReceivedAtServer));
	serverApp.Start(Seconds(0));
}

void configureTCPEchoServer() {
	TcpEchoServerHelper myServer(80);
	serverApp = myServer.Install(wifiApNode);
	wireTCPServer(serverApp);
	serverApp.Start(Seconds(0));
}

void configureTCPPingPongServer() {
	// TCP ping pong is a test for the new base tcp-client and tcp-server applications
	ObjectFactory factory;
	factory.SetTypeId(TCPPingPongServer::GetTypeId());
	factory.Set("Port", UintegerValue(81));

	Ptr<Application> tcpServer = factory.Create<TCPPingPongServer>();
	wifiApNode.Get(0)->AddApplication(tcpServer);

	auto serverApp = ApplicationContainer(tcpServer);
	wireTCPServer(serverApp);
	serverApp.Start(Seconds(0));
}

void configureTCPPingPongClients() {

	ObjectFactory factory;
	factory.SetTypeId(TCPPingPongClient::GetTypeId());
	factory.Set("Interval", TimeValue(MilliSeconds(config.trafficInterval)));
	factory.Set("PacketSize", UintegerValue(config.payloadSize));

	factory.Set("RemoteAddress",
			Ipv4AddressValue(apNodeInterface.GetAddress(0)));
	factory.Set("RemotePort", UintegerValue(81));

	Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable>();

	for (uint16_t i = 0; i < config.Nsta; i++) {

		Ptr<Application> tcpClient = factory.Create<TCPPingPongClient>();
		wifiStaNode.Get(i)->AddApplication(tcpClient);
		auto clientApp = ApplicationContainer(tcpClient);
		wireTCPClient(clientApp, i);

		double random = m_rv->GetValue(0, config.trafficInterval);
		clientApp.Start(MilliSeconds(0 + random));
		//clientApp.Stop(Seconds(simulationTime + 1));
	}
}

void configureTCPIPCameraServer() {
	ObjectFactory factory;
	factory.SetTypeId(TCPIPCameraServer::GetTypeId());
	factory.Set("Port", UintegerValue(82));

	Ptr<Application> tcpServer = factory.Create<TCPIPCameraServer>();
	wifiApNode.Get(0)->AddApplication(tcpServer);

	auto serverApp = ApplicationContainer(tcpServer);
	wireTCPServer(serverApp);
	serverApp.Start(Seconds(0));
//	serverApp.Stop(Seconds(config.simulationTime));
}

void configureTCPIPCameraClients() {

	ObjectFactory factory;
	factory.SetTypeId(TCPIPCameraClient::GetTypeId());
	factory.Set("MotionPercentage",
			DoubleValue(config.ipcameraMotionPercentage));
	factory.Set("MotionDuration",
			TimeValue(Seconds(config.ipcameraMotionDuration)));
	factory.Set("DataRate", UintegerValue(config.ipcameraDataRate));

	factory.Set("PacketSize", UintegerValue(config.payloadSize));

	factory.Set("RemoteAddress",
			Ipv4AddressValue(apNodeInterface.GetAddress(0)));
	factory.Set("RemotePort", UintegerValue(82));

	Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable>();

	for (uint16_t i = 0; i < config.Nsta; i++) {

		Ptr<Application> tcpClient = factory.Create<TCPIPCameraClient>();
		wifiStaNode.Get(i)->AddApplication(tcpClient);
		auto clientApp = ApplicationContainer(tcpClient);
		wireTCPClient(clientApp, i);

		clientApp.Start(MilliSeconds(0));
		//clientApp.Stop(Seconds(config.simulationTime));
	}
}

void configureTCPFirmwareServer() {
	ObjectFactory factory;
	factory.SetTypeId(TCPFirmwareServer::GetTypeId());
	factory.Set("Port", UintegerValue(83));

	factory.Set("FirmwareSize", UintegerValue(config.firmwareSize));
	factory.Set("BlockSize", UintegerValue(config.firmwareBlockSize));
	factory.Set("NewUpdateProbability",
			DoubleValue(config.firmwareNewUpdateProbability));

	Ptr<Application> tcpServer = factory.Create<TCPFirmwareServer>();
	wifiApNode.Get(0)->AddApplication(tcpServer);

	auto serverApp = ApplicationContainer(tcpServer);
	wireTCPServer(serverApp);
	serverApp.Start(Seconds(0));
//	serverApp.Stop(Seconds(config.simulationTime));
}

void configureTCPFirmwareClients() {

	ObjectFactory factory;
	factory.SetTypeId(TCPFirmwareClient::GetTypeId());
	factory.Set("CorruptionProbability",
			DoubleValue(config.firmwareCorruptionProbability));
	factory.Set("VersionCheckInterval",
			TimeValue(MilliSeconds(config.firmwareVersionCheckInterval)));
	factory.Set("PacketSize", UintegerValue(config.payloadSize));

	factory.Set("RemoteAddress",
			Ipv4AddressValue(apNodeInterface.GetAddress(0)));
	factory.Set("RemotePort", UintegerValue(83));

	Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable>();

	for (uint16_t i = 0; i < config.Nsta; i++) {

		Ptr<Application> tcpClient = factory.Create<TCPFirmwareClient>();
		wifiStaNode.Get(i)->AddApplication(tcpClient);
		auto clientApp = ApplicationContainer(tcpClient);
		wireTCPClient(clientApp, i);

		double random = m_rv->GetValue(0, config.trafficInterval);
		clientApp.Start(MilliSeconds(0 + random));
		clientApp.Stop(Seconds(config.simulationTime));
	}
}

void configureTCPSensorServer() {
	ObjectFactory factory;
	factory.SetTypeId(TCPSensorServer::GetTypeId());
	factory.Set("Port", UintegerValue(84));

	Ptr<Application> tcpServer = factory.Create<TCPSensorServer>();
	wifiApNode.Get(0)->AddApplication(tcpServer);

	auto serverApp = ApplicationContainer(tcpServer);
	wireTCPServer(serverApp);
	serverApp.Start(Seconds(0));
//	serverApp.Stop(Seconds(config.simulationTime));
}

void configureTCPSensorClients() {

	ObjectFactory factory;
	factory.SetTypeId(TCPSensorClient::GetTypeId());

	factory.Set("Interval", TimeValue(MilliSeconds(config.trafficInterval)));
	factory.Set("PacketSize", UintegerValue(config.payloadSize));
	factory.Set("MeasurementSize", UintegerValue(config.sensorMeasurementSize));

	factory.Set("RemoteAddress",
			Ipv4AddressValue(apNodeInterface.GetAddress(0)));
	factory.Set("RemotePort", UintegerValue(84));

	Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable>();

	for (uint16_t i = 0; i < config.Nsta; i++) {

		Ptr<Application> tcpClient = factory.Create<TCPSensorClient>();
		wifiStaNode.Get(i)->AddApplication(tcpClient);
		auto clientApp = ApplicationContainer(tcpClient);
		wireTCPClient(clientApp, i);

		double random = m_rv->GetValue(0, config.trafficInterval);
		clientApp.Start(MilliSeconds(0 + random));
		clientApp.Stop(Seconds(config.simulationTime));
	}
}

void wireTCPServer(ApplicationContainer serverApp) {
	serverApp.Get(0)->TraceConnectWithoutContext("Rx",
			MakeCallback(&tcpPacketReceivedAtServer));
	serverApp.Get(0)->TraceConnectWithoutContext("Retransmission",
			MakeCallback(&tcpRetransmissionAtServer));
	serverApp.Get(0)->TraceConnectWithoutContext("PacketDropped",
			MakeCallback(&tcpPacketDroppedAtServer));
	serverApp.Get(0)->TraceConnectWithoutContext("TCPStateChanged",
			MakeCallback(&tcpStateChangeAtServer));

	if (config.trafficType == "tcpipcamera") {
		serverApp.Get(0)->TraceConnectWithoutContext("DataReceived",
				MakeCallback(&tcpIPCameraDataReceivedAtServer));
	}
}

void wireTCPClient(ApplicationContainer clientApp, int i) {

	clientApp.Get(0)->TraceConnectWithoutContext("Tx",
			MakeCallback(&NodeEntry::OnTcpPacketSent, nodes[i]));
	clientApp.Get(0)->TraceConnectWithoutContext("Rx",
			MakeCallback(&NodeEntry::OnTcpEchoPacketReceived, nodes[i]));

	clientApp.Get(0)->TraceConnectWithoutContext("CongestionWindow",
			MakeCallback(&NodeEntry::OnTcpCongestionWindowChanged, nodes[i]));
	clientApp.Get(0)->TraceConnectWithoutContext("RTO",
			MakeCallback(&NodeEntry::OnTcpRTOChanged, nodes[i]));
	clientApp.Get(0)->TraceConnectWithoutContext("RTT",
			MakeCallback(&NodeEntry::OnTcpRTTChanged, nodes[i]));
	clientApp.Get(0)->TraceConnectWithoutContext("SlowStartThreshold",
			MakeCallback(&NodeEntry::OnTcpSlowStartThresholdChanged, nodes[i]));
	clientApp.Get(0)->TraceConnectWithoutContext("EstimatedBW",
			MakeCallback(&NodeEntry::OnTcpEstimatedBWChanged, nodes[i]));

	clientApp.Get(0)->TraceConnectWithoutContext("TCPStateChanged",
			MakeCallback(&NodeEntry::OnTcpStateChanged, nodes[i]));
	clientApp.Get(0)->TraceConnectWithoutContext("Retransmission",
			MakeCallback(&NodeEntry::OnTcpRetransmission, nodes[i]));

	clientApp.Get(0)->TraceConnectWithoutContext("PacketDropped",
			MakeCallback(&NodeEntry::OnTcpPacketDropped, nodes[i]));

	if (config.trafficType == "tcpfirmware") {
		clientApp.Get(0)->TraceConnectWithoutContext("FirmwareUpdated",
				MakeCallback(&NodeEntry::OnTcpFirmwareUpdated, nodes[i]));
	} else if (config.trafficType == "tcpipcamera") {
		clientApp.Get(0)->TraceConnectWithoutContext("DataSent",
				MakeCallback(&NodeEntry::OnTcpIPCameraDataSent, nodes[i]));
		clientApp.Get(0)->TraceConnectWithoutContext("StreamStateChanged",
				MakeCallback(&NodeEntry::OnTcpIPCameraStreamStateChanged,
						nodes[i]));
	}
}

void configureTCPEchoClients() {
	TcpEchoClientHelper clientHelper(apNodeInterface.GetAddress(0), 80); //address of remote node
	clientHelper.SetAttribute("MaxPackets", UintegerValue(4294967295u));
	clientHelper.SetAttribute("Interval",
			TimeValue(MilliSeconds(config.trafficInterval)));
	//clientHelper.SetAttribute("IntervalDeviation", TimeValue(MilliSeconds(config.trafficIntervalDeviation)));
	clientHelper.SetAttribute("PacketSize", UintegerValue(config.payloadSize));

	Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable>();

	for (uint16_t i = 0; i < config.Nsta; i++) {
		ApplicationContainer clientApp = clientHelper.Install(
				wifiStaNode.Get(i));
		wireTCPClient(clientApp, i);

		double random = m_rv->GetValue(0, config.trafficInterval);
		clientApp.Start(MilliSeconds(0 + random));
		//clientApp.Stop(Seconds(simulationTime + 1));
	}
}

void configureUDPClients() {
	//Application start time
	Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable>();

	UdpClientHelper myClient(apNodeInterface.GetAddress(0), 9); //address of remote node
	myClient.SetAttribute("MaxPackets", config.maxNumberOfPackets);
	myClient.SetAttribute("PacketSize", UintegerValue(config.payloadSize));
	traffic_sta.clear();
	ifstream trafficfile(config.TrafficPath);
	if (trafficfile.is_open()) {
		uint16_t sta_id;
		float sta_traffic;
		for (uint16_t kk = 0; kk < config.Nsta; kk++) {
			trafficfile >> sta_id;
			trafficfile >> sta_traffic;
			traffic_sta.insert(std::make_pair(sta_id, sta_traffic)); //insert data
			//cout << "sta_id = " << sta_id << " sta_traffic = " << sta_traffic << "\n";
		}
		trafficfile.close();
	} else
		cout << "Unable to open traffic file \n";

	double randomStart = 0.0;
	for (std::map<uint16_t, float>::iterator it = traffic_sta.begin();
			it != traffic_sta.end(); ++it) {
		std::ostringstream intervalstr;
		intervalstr << (config.payloadSize * 8) / (it->second * 1000000);
		std::string intervalsta = intervalstr.str();

		//config.trafficInterval = UintegerValue (Time (intervalsta));

		myClient.SetAttribute("Interval", TimeValue(Time(intervalsta))); // TODO add to nodeEntry and visualize
		randomStart = m_rv->GetValue(0,
				(config.payloadSize * 8) / (it->second * 1000000));
		ApplicationContainer clientApp = myClient.Install(
				wifiStaNode.Get(it->first));
		clientApp.Get(0)->TraceConnectWithoutContext("Tx",
				MakeCallback(&NodeEntry::OnUdpPacketSent, nodes[it->first]));
		clientApp.Start(Seconds(1 + randomStart));
		//clientApp.Stop (Seconds (config.simulationTime+1)); //with this throughput is smaller
	}
	AppStartTime = Simulator::Now().GetSeconds() + 1;
	//Simulator::Stop (Seconds (config.simulationTime+1));
}

void configureUDPEchoClients() {
	UdpEchoClientHelper clientHelper(apNodeInterface.GetAddress(0), 9); //address of remote node
	clientHelper.SetAttribute("MaxPackets", UintegerValue(config.TrafficPackets));
	clientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(config.trafficInterval)));
	//clientHelper.SetAttribute("IntervalDeviation", TimeValue(MilliSeconds(config.trafficIntervalDeviation)));
	clientHelper.SetAttribute("PacketSize", UintegerValue(config.payloadSize));

	Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable>();
	const double configuredStartSpread = config.trafficStartSpread == 4294967295u
		? double(config.trafficInterval) : double(config.trafficStartSpread);
	const double startSpread = std::min(configuredStartSpread,
		config.simulationTime * 1000.0);
	std::cout << "UDP Echo initial start spread: " << startSpread << " ms"
		<< (config.trafficStartSpread == 4294967295u ? " (legacy interval-derived)" : " (explicit)")
		<< std::endl;

	for (uint16_t i = 0; i < config.Nsta; i++) {
		ApplicationContainer clientApp = clientHelper.Install(
				wifiStaNode.Get(i));
		clientApp.Get(0)->TraceConnectWithoutContext("Tx",
				MakeCallback(&NodeEntry::OnUdpPacketSent, nodes[i]));
		clientApp.Get(0)->TraceConnectWithoutContext("Rx",
				MakeCallback(&NodeEntry::OnUdpEchoPacketReceived, nodes[i]));

		double random = startSpread == 0 ? 0 : m_rv->GetValue(0, startSpread);
		clientApp.Start(MilliSeconds(0 + random));
		//clientApp.Stop(Seconds(simulationTime + 1));
	}
}

Time timeIdleArray[MaxSta];
Time timeRxArray[MaxSta];
Time timeTxArray[MaxSta];
Time timeSleepArray[MaxSta];
Time timeCollisionArray[MaxSta];

Time timeIdleNotAssociated[MaxSta];
Time timeRxNotAssociated[MaxSta];
Time timeTxNotAssociated[MaxSta];
Time timeSleepNotAssociated[MaxSta];
Time timeCollisionNotAssociated[MaxSta];

double dist[MaxSta];

//it prints the information regarding the state of the device
void PhyStateTrace(std::string context, Time start, Time duration,
		enum WifiPhy::State state) {

	/*Get the number of the node from the context*/
	/*context = "/NodeList/"+strSTA+"/DeviceList/'*'/Phy/$ns3::YansWifiPhy/State/State"*/
	unsigned first = context.find("t/");
	unsigned last = context.find("/D");
	string strNew = context.substr((first + 2), (last - first - 2));

	int node = std::stoi(strNew);

	if (nodes[node]->isAssociated)
	{
		switch (state)
		{
		case WifiPhy::State::SLEEP: //Sleep
			timeSleepArray[node] = timeSleepArray[node] + duration;
			//NS_LOG_UNCOND(to_string(node + 1) + ",SLEEP," + to_string(start.GetMicroSeconds()) + " " + to_string(duration.GetMicroSeconds()));
			break;
		case WifiPhy::State::IDLE: //Idle
			timeIdleArray[node] = timeIdleArray[node] + duration;
			//NS_LOG_UNCOND(to_string(node + 1) + ",IDLE," + to_string(start.GetMicroSeconds()) + " " + to_string(duration.GetMicroSeconds()));
			break;
		case WifiPhy::State::TX: //Tx
			timeTxArray[node] = timeTxArray[node] + duration;
			//NS_LOG_UNCOND (to_string(node+1) + ",TX," + to_string(start.GetMicroSeconds()) + " " + to_string(duration.GetMicroSeconds()));
			break;
		case WifiPhy::State::RX: //Rx
			timeRxArray[node] = timeRxArray[node] + duration;
			//NS_LOG_UNCOND (to_string(node+1) + ",RX," + to_string(start.GetMicroSeconds()) + " " + to_string(duration.GetMicroSeconds()));
			break;
		case WifiPhy::State::CCA_BUSY: //CCA_BUSY
			timeCollisionArray[node] = timeCollisionArray[node] + duration;
			//NS_LOG_UNCOND (to_string(node+1) + ",CCA_BUSY," + to_string(start.GetMicroSeconds()) + " " + to_string(duration.GetMicroSeconds()));
			break;
		}
	}
	else
	{
		switch (state)
		{
		case WifiPhy::State::SLEEP: //Sleep
			timeSleepNotAssociated[node] = timeSleepNotAssociated[node] + duration;
			//NS_LOG_UNCOND(to_string(node + 1) + ",SLEEP," + to_string(start.GetMicroSeconds()) + " " + to_string(duration.GetMicroSeconds()));
			break;
		case WifiPhy::State::IDLE: //Idle
			timeIdleNotAssociated[node] = timeIdleNotAssociated[node] + duration;
			//NS_LOG_UNCOND(to_string(node + 1) + ",IDLE," + to_string(start.GetMicroSeconds()) + " " + to_string(duration.GetMicroSeconds()));
			break;
		case WifiPhy::State::TX: //Tx
			timeTxNotAssociated[node] = timeTxNotAssociated[node] + duration;
			//NS_LOG_UNCOND (to_string(node+1) + ",TX," + to_string(start.GetMicroSeconds()) + " " + to_string(duration.GetMicroSeconds()));
			break;
		case WifiPhy::State::RX: //Rx
			timeRxNotAssociated[node] = timeRxNotAssociated[node] + duration;
			//NS_LOG_UNCOND (to_string(node+1) + ",RX," + to_string(start.GetMicroSeconds()) + " " + to_string(duration.GetMicroSeconds()));
			break;
		case WifiPhy::State::CCA_BUSY: //CCA_BUSY
			timeCollisionNotAssociated[node] = timeCollisionNotAssociated[node] + duration;
			//NS_LOG_UNCOND (to_string(node+1) + ",CCA_BUSY," + to_string(start.GetMicroSeconds()) + " " + to_string(duration.GetMicroSeconds()));
			break;
		}
	}
}

int main(int argc, char *argv[]) {
	//LogComponentEnable ("UdpServer", LOG_INFO);
	 LogComponentEnable ("UdpEchoServerApplication", LOG_INFO);
	 LogComponentEnable ("UdpEchoClientApplication", LOG_INFO);

	//LogComponentEnable ("ApWifiMac", LOG_DEBUG);
	//LogComponentEnable ("StaWifiMac", LOG_DEBUG);
	//LogComponentEnable ("EdcaTxopN", LOG_DEBUG);

	bool OutputPosition = true;
	config = Configuration(argc, argv);

    try {
        config.rps = configureRAW(config.rps, config.RAWConfigFile);
    } catch (const std::exception &error) {
        std::cerr << "RAW configuration error: " << error.what()
                  << ". No simulation was run." << std::endl;
        return 2;
    }
    if (config.ValidateRawOnly) {
        return 0;
    }

    if (!std::isfinite(config.AssociationTimeout) || config.AssociationTimeout <= 0 ||
        !std::isfinite(config.simulationTime) || config.simulationTime <= 0 ||
        config.trafficInterval == 0 || config.TrafficPackets == 0 ||
        !std::isfinite(config.visualizerSamplingInterval) || config.visualizerSamplingInterval <= 0) {
        std::cerr << "Invalid association deadline or traffic parameters" << std::endl;
        return 2;
    }
    if (config.RotateRawPages) {
        const bool sliced = config.pageSliceCount == 2 && config.pageSliceLength == 16;
        config.pagePeriod = config.rps.rpsset.size() * (sliced ? 2 : 1);
        if (config.rps.rpsset.empty() || config.rps.rpsset.size() > 4 ||
            (!sliced && (config.pageSliceCount != 0 || config.pageSliceLength != 1)) ||
            config.blockOffset != 0 || config.timOffset != 0) {
            std::cerr << "Rotation requires 1..4 RPS, whole pages or two 16-block slices, and zero offsets" << std::endl;
            return 2;
        }
    }
	configurePageSlice ();
	configureTIM ();
	//checkRawAndTimConfiguration ();

	config.NSSFile = "results/" + config.trafficType + "_" + std::to_string(config.Nsta)
			+ "sta_" + std::to_string(config.NGroup) + "Group_"
			+ std::to_string(config.NRawSlotNum) + "slots_"
			+ std::to_string(config.payloadSize) + "payload_"
			+ std::to_string(config.totaltraffic) + "Mbps_"
			+ std::to_string(config.BeaconInterval) + "BI" + ".nss";

    if (!config.ResultPrefix.empty()) {
        config.NSSFile = config.ResultPrefix + ".nss";
        std::ofstream probe((config.ResultPrefix + "-summary.csv").c_str());
        if (!probe) { std::cerr << "Cannot open ResultPrefix" << std::endl; return 2; }
        if (config.trafficType == "udpecho") NodeEntry::OpenApplicationEvents(config.ResultPrefix);
    }
	stats = Statistics(config.Nsta);
	eventManager = SimulationEventManager(config.visualizerIP,
			config.visualizerPort, config.NSSFile);
	uint32_t totalRawGroups(0);
	for (int i = 0; i < config.rps.rpsset.size(); i++) {
		int nRaw = config.rps.rpsset[i]->GetNumberOfRawGroups();
		totalRawGroups += nRaw;
		//cout << "Total raw groups after rps " << i << " is " << totalRawGroups << endl;
		for (int j = 0; j < nRaw; j++) {
			config.totalRawSlots += config.rps.rpsset[i]->GetRawAssigmentObj(j).GetSlotNum();
			//cout << "Total slots after group " << j << " is " << totalRawSlots << endl;
		}

	}
	transmissionsPerTIMGroupAndSlotFromAPSinceLastInterval = vector<long>(
			config.totalRawSlots, 0);
	transmissionsPerTIMGroupAndSlotFromSTASinceLastInterval = vector<long>(
			config.totalRawSlots, 0);

	RngSeedManager::SetSeed(config.seed);

	wifiStaNode.Create(config.Nsta);
	wifiApNode.Create(1);

	YansWifiChannelHelper channelBuilder = YansWifiChannelHelper();
	channelBuilder.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
			"Exponent", DoubleValue(3.76), "ReferenceLoss", DoubleValue(8.0),
			"ReferenceDistance", DoubleValue(1.0));
	channelBuilder.SetPropagationDelay(
			"ns3::ConstantSpeedPropagationDelayModel");
	Ptr<YansWifiChannel> channel = channelBuilder.Create();
	channel->TraceConnectWithoutContext("Transmission",
			MakeCallback(&onChannelTransmission)); //TODO

	YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
	phy.SetErrorRateModel("ns3::YansErrorRateModel");
	phy.SetChannel(channel);
	phy.Set("ShortGuardEnabled", BooleanValue(false));
	phy.Set("ChannelWidth", UintegerValue(getBandwidth(config.DataMode))); // changed
	phy.Set("EnergyDetectionThreshold", DoubleValue(-110.0));
	phy.Set("CcaMode1Threshold", DoubleValue(-113.0));
	phy.Set("TxGain", DoubleValue(0.0));
	phy.Set("RxGain", DoubleValue(0.0));
	phy.Set("TxPowerLevels", UintegerValue(1));
	phy.Set("TxPowerEnd", DoubleValue(0.0));
	phy.Set("TxPowerStart", DoubleValue(0.0));
	phy.Set("RxNoiseFigure", DoubleValue(6.8));
	phy.Set("LdpcEnabled", BooleanValue(true));
	phy.Set("S1g1MfieldEnabled", BooleanValue(config.S1g1MfieldEnabled));

	WifiHelper wifi = WifiHelper::Default();
	wifi.SetStandard(WIFI_PHY_STANDARD_80211ah);
	S1gWifiMacHelper mac = S1gWifiMacHelper::Default();

	Ssid ssid = Ssid("ns380211ah");
	StringValue DataRate;
	DataRate = StringValue(getWifiMode(config.DataMode)); // changed

	wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",DataRate, "ControlMode", DataRate);
    //wifi.SetRemoteStationManager("ns3::ArfWifiManager");


	mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing",
			BooleanValue(false));

	NetDeviceContainer staDevice;
	staDevice = wifi.Install(phy, mac, wifiStaNode);

	mac.SetType ("ns3::ApWifiMac",
	                 "Ssid", SsidValue (ssid),
	                 "BeaconInterval", TimeValue (MicroSeconds(config.BeaconInterval)),
	                 "NRawStations", UintegerValue (config.NRawSta),
	                 "RPSsetup", RPSVectorValue (config.rps),
	                 "PageSliceSet", pageSliceValue (config.pageS),
	                 "TIMSet", TIMValue (config.tim),
                     "RotateRawPages", BooleanValue(config.RotateRawPages)
	               );

	phy.Set("TxGain", DoubleValue(3.0));
	phy.Set("RxGain", DoubleValue(3.0));
	phy.Set("TxPowerLevels", UintegerValue(1));
	phy.Set("TxPowerEnd", DoubleValue(30.0));
	phy.Set("TxPowerStart", DoubleValue(30.0));
	phy.Set("RxNoiseFigure", DoubleValue(6.8));

	apDevice = wifi.Install(phy, mac, wifiApNode);
    if (config.RotateRawPages && !config.ResultPrefix.empty()) {
        raw_observer::Install(staDevice, apDevice, config.ResultPrefix);
    }

	Config::Set(
			"/NodeList/*/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_EdcaTxopN/Queue/MaxPacketNumber",
			UintegerValue(10));
	Config::Set(
			"/NodeList/*/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_EdcaTxopN/Queue/MaxDelay",
			TimeValue(NanoSeconds(6000000000000)));
    {
        // The AP buffers downlink for every STA until its page/slot; a 10-packet
        // limit (meant for STAs) would drop most replies at scale.
        PointerValue edca;
        DynamicCast<RegularWifiMac>(DynamicCast<WifiNetDevice>(apDevice.Get(0))->GetMac())
            ->GetAttribute("BE_EdcaTxopN", edca);
        edca.Get<EdcaTxopN>()->GetEdcaQueue()->SetMaxSize(std::max<uint32_t>(10, config.Nsta));
    }
/*
	string DataModeCamera = "OfdmRate650KbpsBW2MHz";
	StringValue aa = StringValue(DataModeCamera);
	for (uint16_t k = 0; k < config.Nsta; k++) {
		std::ostringstream APSTA;
		APSTA << k;
		std::string strAP = APSTA.str();
		Config::Set(
				"/NodeList/" + strAP
						+ "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/$ns3::ConstantRateWifiManager/DataMode",
				aa);
		Config::Set(
				"/NodeList/" + strAP
						+ "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/$ns3::ConstantRateWifiManager/ControlMode",
				aa);
		Config::Set(
				"/NodeList/" + strAP
						+ "/DeviceList/0/$ns3::WifiNetDevice/Phy/$ns3::YansWifiPhy/ChannelWidth",
				UintegerValue(2));
	}
*/
	std::ostringstream oss;
	oss << "/NodeList/" << wifiApNode.Get(0)->GetId()
			<< "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::ApWifiMac/";
	Config::ConnectWithoutContext(oss.str() + "RpsIndex", MakeCallback(&RpsIndexTrace));
	Config::ConnectWithoutContext(oss.str() + "RawGroup", MakeCallback(&RawGroupTrace));
	Config::ConnectWithoutContext(oss.str() + "RawSlot", MakeCallback(&RawSlotTrace));

	// mobility.
	MobilityHelper mobility;
	double xpos = std::stoi(config.rho, nullptr, 0);
	double ypos = xpos;
	mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator", "X",
			StringValue(std::to_string(xpos)), "Y",
			StringValue(std::to_string(ypos)), "rho", StringValue(config.rho));
	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	mobility.Install(wifiStaNode);

	MobilityHelper mobilityAp;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<
			ListPositionAllocator>();
	positionAlloc->Add(Vector(xpos, ypos, 0.0));
	mobilityAp.SetPositionAllocator(positionAlloc);
	mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	mobilityAp.Install(wifiApNode);

	/*

	 MobilityHelper mobilityApCamera;
	 Ptr<ListPositionAllocator> positionAllocAp = CreateObject<ListPositionAllocator> ();
	 positionAllocAp->Add (Vector (xpos, ypos, 0.0));
	 mobilityApCamera.SetPositionAllocator (positionAllocAp);
	 mobilityApCamera.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	 mobilityApCamera.Install (wifiApNode);

	 float deltaAngle = 2* M_PI / (config.tcpipcameraEnd - config.tcpipcameraStart +1);
	 float angle = 0.0;
	 double x = 0.0;
	 double y = 0.0;

	 double Distance = 50.0;


	 Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable> ();


	 for (int i = config.tcpipcameraStart; i <= config.tcpipcameraEnd; i++)
	 {
	 x = cos(angle) * Distance + xpos;
	 y = sin(angle) * Distance + ypos;

	 MobilityHelper mobilityCamera;
	 Ptr<ListPositionAllocator> positionAllocSta = CreateObject<ListPositionAllocator> ();
	 positionAllocSta->Add(Vector(x, y, 0.0));
	 mobilityCamera.SetPositionAllocator(positionAllocSta);
	 mobilityCamera.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	 mobilityCamera.Install(wifiStaNode.Get(i));
	 angle += deltaAngle;
	 }

	 */

	/* Internet stack*/
	InternetStackHelper stack;
	stack.Install(wifiApNode);
	stack.Install(wifiStaNode);

	Ipv4AddressHelper address;

	address.SetBase("192.168.0.0", "255.255.0.0");

	staNodeInterface = address.Assign(staDevice);
	apNodeInterface = address.Assign(apDevice);

	//trace association
	std::cout << "Configuring trace sources..." << std::endl;
	for (uint16_t kk = 0; kk < config.Nsta; kk++) {
		std::ostringstream STA;
		STA << kk;
		std::string strSTA = STA.str();

		assoc_record *m_assocrecord = new assoc_record;
		m_assocrecord->setstaid(kk);
		ConnectNodeTrace(
				"/NodeList/" + strSTA
						+ "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Assoc",
				MakeCallback(&assoc_record::SetAssoc, m_assocrecord));
		ConnectNodeTrace(
				"/NodeList/" + strSTA
						+ "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/DeAssoc",
				MakeCallback(&assoc_record::UnsetAssoc, m_assocrecord));
		assoc_vector.push_back(m_assocrecord);
	}

    // All peers are in the same /16 on one Wi-Fi link. Connected routes
    // installed by address.Assign suffice; global SPF is unnecessary here.
    std::cout << "Using connected routes for the single Wi-Fi subnet" << std::endl;
	std::cout << "Populating ARP cache..." << std::endl;
	PopulateArpCache();

	// configure tracing for associations & other metrics
	std::cout << "Configuring trace sinks for nodes..." << std::endl;
	configureNodes(wifiStaNode, staDevice);

	ConnectNodeTrace(
			"/NodeList/" + std::to_string(config.Nsta)
					+ "/DeviceList/0/$ns3::WifiNetDevice/Phy/PhyRxDropWithReason",
			MakeCallback(&OnAPPhyRxDrop));
	ConnectNodeTrace(
			"/NodeList/" + std::to_string(config.Nsta)
					+ "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::ApWifiMac/PacketToTransmitReceivedFromUpperLayer",
			MakeCallback(&OnAPPacketToTransmitReceived));

	Ptr<MobilityModel> mobility1 =
			wifiApNode.Get(0)->GetObject<MobilityModel>();
	Vector apposition = mobility1->GetPosition();
	if (OutputPosition) {
		uint32_t i = 0;
		while (i < config.Nsta) {
			Ptr<MobilityModel> mobility = wifiStaNode.Get(i)->GetObject<
					MobilityModel>();
			Vector position = mobility->GetPosition();
			nodes[i]->x = position.x;
			nodes[i]->y = position.y;
			std::cout << "Sta node#" << i << ", " << "position = " << position
					<< std::endl;
			dist[i] = mobility->GetDistanceFrom(
					wifiApNode.Get(0)->GetObject<MobilityModel>());
			i++;
		}
		std::cout << "AP node, position = " << apposition << std::endl;
	}

	/*Print of the state of the stations*/
	for (uint32_t i = 0; i < config.Nsta; i++) {
		std::ostringstream STA;
		STA << i;
		std::string strSTA = STA.str();

		ConnectNodeTrace(
				"/NodeList/" + strSTA
						+ "/DeviceList/*/Phy/$ns3::YansWifiPhy/State/State",
				MakeCallback(&PhyStateTrace));
	}

	eventManager.onStartHeader();
	eventManager.onStart(config);
	if (config.rps.rpsset.size() > 0)
		for (uint32_t i = 0; i < config.rps.rpsset.size(); i++)
			for (uint32_t j = 0;
					j < config.rps.rpsset[i]->GetNumberOfRawGroups(); j++)
				eventManager.onRawConfig(i, j,
						config.rps.rpsset[i]->GetRawAssigmentObj(j));

	for (uint32_t i = 0; i < config.Nsta; i++)
		eventManager.onSTANodeCreated(*nodes[i]);

	eventManager.onAPNodeCreated(apposition.x, apposition.y);
	eventManager.onStatisticsHeader();

	sendStatistics(true);

	associationDeadline = Simulator::Schedule(Seconds(config.AssociationTimeout), [] {
        std::cerr << "Association timeout: " << GetAssocNum() << "/" << config.Nsta << std::endl;
        Simulator::Stop();
    });
	Simulator::Run();
    NodeEntry::CloseApplicationEvents();

    // Snapshot without queue Cleanup: do not change packet fate at shutdown.
    if (!config.ResultPrefix.empty()) {
        std::ofstream pending((config.ResultPrefix + "-ap-pending.log").c_str());
        NS_ABORT_MSG_IF(!pending, "Cannot open AP pending diagnostics");
        pending << "t_ns=" << Simulator::Now().GetNanoSeconds() << "\n";
        auto apMac = DynamicCast<RegularWifiMac>(DynamicCast<WifiNetDevice>(apDevice.Get(0))->GetMac());
        for (const char *name : {"BE_EdcaTxopN", "BK_EdcaTxopN", "VI_EdcaTxopN", "VO_EdcaTxopN"}) {
            PointerValue value;
            apMac->GetAttribute(name, value);
            pending << "AP_AC name=" << name << "\n";
            value.Get<EdcaTxopN>()->WritePendingDiagnostics(pending);
        }
    }

	// Visualizer throughput
	int pay = 0, totalSuccessfulPackets = 0, totalSentPackets = 0, totalPacketsEchoed = 0;
	for (int i = 0; i < config.Nsta; i++)
	{
		totalSuccessfulPackets += stats.get(i).NumberOfSuccessfulPackets;
		totalSentPackets += stats.get(i).NumberOfSentPackets;
		totalPacketsEchoed += stats.get(i).NumberOfSuccessfulRoundtripPackets;
		pay += stats.get(i).TotalPacketPayloadSize;
		cout << i << " sent: " << stats.get(i).NumberOfSentPackets
				<< " ; delivered: " << stats.get(i).NumberOfSuccessfulPackets
				<< " ; echoed: " << stats.get(i).NumberOfSuccessfulRoundtripPackets
				<< "; packetloss: "
				<< stats.get(i).GetPacketLoss(config.trafficType) << endl;
	}

	if (config.trafficType == "udp" && trafficStarted)
	{
		double throughput = 0;
		uint32_t totalPacketsThrough =
				DynamicCast<UdpServer>(serverApp.Get(0))->GetReceived();
		throughput = totalPacketsThrough * config.payloadSize * 8
				/ (config.simulationTime * 1000000.0);
		cout << "totalPacketsThrough " << totalPacketsThrough << " ++my "
				<< totalSuccessfulPackets << endl;
		cout << "throughput " << throughput << " ++my "
				<< pay * 8. / (config.simulationTime * 1000000.0) << endl;
		std::cout << "datarate" << "\t" << "throughput" << std::endl;
		std::cout << config.datarate << "\t" << throughput << " Mbit/s"
				<< std::endl;

	}
	else if (config.trafficType == "udpecho")
	{
		double ulThroughput = 0, dlThroughput = 0;
		ulThroughput = totalSuccessfulPackets * config.payloadSize * 8 / (config.simulationTime * 1000000.0);
		dlThroughput = totalPacketsEchoed * config.payloadSize * 8 / (config.simulationTime * 1000000.0);
		cout << "totalPacketsSent " << totalSentPackets << endl;
		cout << "totalPacketsDelivered " << totalSuccessfulPackets << endl;
		cout << "totalPacketsEchoed " << totalPacketsEchoed << endl;
		cout << "UL packets lost " << totalSentPackets - totalSuccessfulPackets << endl;
		cout << "DL packets lost " << totalSuccessfulPackets - totalPacketsEchoed << endl;
		cout << "Total packets lost " << totalSentPackets - totalPacketsEchoed << endl;

		/*cout << "uplink throughput Mbit/s " << ulThroughput << endl;
		cout << "downlink throughput Mbit/s " << dlThroughput << endl;*/

		double throughput = (totalSuccessfulPackets + totalPacketsEchoed) * config.payloadSize * 8 / (config.simulationTime * 1000000.0);
		cout << "total throughput Kbit/s " << throughput * 1000 << endl;

		std::cout << "datarate" << "\t" << "throughput" << std::endl;
		std::cout << config.datarate << "\t" << throughput * 1000 << " Kbit/s" << std::endl;
	}
    if (totalSentPackets > 0) {
        cout << "total packet loss % " << 100 - 100. * totalPacketsEchoed / totalSentPackets << endl;
    } else {
        cout << "total packet loss % unavailable (no packets sent)" << endl;
    }

	ofstream risultati;
	string addressresults = config.ResultPrefix.empty() ? config.OutputPath + "moreinfo.txt" : config.ResultPrefix + "-phy.csv";
	risultati.open(addressresults.c_str(), ios::out | ios::trunc);

    risultati << "sta_index,distance_m,phy_rx_associated_s,phy_rx_not_associated_s,phy_idle_associated_s,phy_idle_not_associated_s,phy_tx_associated_s,phy_tx_not_associated_s,phy_sleep_associated_s,phy_sleep_not_associated_s,phy_cca_busy_associated_s,phy_cca_busy_not_associated_s" << std::endl;
    int i = 0;
    string spazio = ",";

    while (i < config.Nsta) {

        risultati << i << spazio << dist[i] << spazio << timeRxArray[i].GetSeconds() << ',' << timeRxNotAssociated[i].GetSeconds() << ',' << timeIdleArray[i].GetSeconds() << ',' << timeIdleNotAssociated[i].GetSeconds() << ',' << timeTxArray[i].GetSeconds() << ',' << timeTxNotAssociated[i].GetSeconds() << ',' << timeSleepArray[i].GetSeconds() << ',' << timeSleepNotAssociated[i].GetSeconds() << ',' << timeCollisionArray[i].GetSeconds() << ',' << timeCollisionNotAssociated[i].GetSeconds() << std::endl;
        /*
         cout << "================== Sleep " << stats.get(i).TotalSleepTime.GetSeconds() << endl;
         cout << "================== Tx " << stats.get(i).TotalTxTime.GetSeconds() << endl;
         cout << "================== Rx " << stats.get(i).TotalRxTime.GetSeconds() << endl;
         cout << "+++++++++++++++++++IDLE " << stats.get(i).TotalIdleTime.GetSeconds() << endl;
         cout << "ooooooooooooooooooo TOTENERGY " <<  stats.get(i).GetTotalEnergyConsumption() << " mW" << endl;
         cout << "Rx+Idle ENERGY " <<  stats.get(i).EnergyRxIdle << " mW" << endl;
         cout << "Tx ENERGY " <<  stats.get(i).EnergyTx << " mW" << endl;*/

        i++;
    }

	risultati.close();
    const uint32_t associated = GetAssocNum();
    auto apManager = DynamicCast<WifiNetDevice>(apDevice.Get(0))->GetRemoteStationManager();
    uint32_t apAssociated = 0;
    std::ofstream associationAudit;
    if (!config.ResultPrefix.empty()) {
        associationAudit.open((config.ResultPrefix + "-association.csv").c_str());
        NS_ABORT_MSG_IF(!associationAudit, "Cannot open association audit");
        associationAudit << "aid,sta_associated,ap_associated,ap_waiting\n";
    }
    for (uint32_t n = 0; n < config.Nsta; ++n) {
        Mac48Address address = Mac48Address::ConvertFrom(staDevice.Get(n)->GetAddress());
        const bool confirmed = apManager->IsAssociated(address);
        apAssociated += confirmed;
        if (associationAudit.is_open())
            associationAudit << n + 1 << ',' << assoc_vector[n]->GetAssoc() << ','
                             << confirmed << ',' << apManager->IsWaitAssocTxOk(address) << '\n';
    }
    std::cout << "RUN_STATUS associated=" << associated << "/" << config.Nsta
              << " ap_associated=" << apAssociated << "/" << config.Nsta
              << " traffic_started=" << trafficStarted
              << " end_s=" << Simulator::Now().GetSeconds() << std::endl;
    if (!config.ResultPrefix.empty()) {
        std::ofstream summary((config.ResultPrefix + "-summary.csv").c_str());
        summary << "aid,associated,sent,delivered,echoed\n";
        for (uint32_t n = 0; n < config.Nsta; ++n) {
            summary << n + 1 << ',' << assoc_vector[n]->GetAssoc() << ','
                    << stats.get(n).NumberOfSentPackets << ','
                    << stats.get(n).NumberOfSuccessfulPackets << ','
                    << stats.get(n).NumberOfSuccessfulRoundtripPackets << '\n';
        }
        std::ofstream run((config.ResultPrefix + "-run.csv").c_str());
        run << "stations,associated,traffic_started,association_s,traffic_window_s,cooldown_s,end_s,sent,delivered,echoed\n";
        run << config.Nsta << ',' << associated << ',' << trafficStarted << ',';
        if (trafficStarted) run << stats.TimeWhenEverySTAIsAssociated.GetSeconds();
        run << ',' << config.simulationTime << ',' << config.CoolDownPeriod << ','
            << Simulator::Now().GetSeconds() << ',' << totalSentPackets << ','
            << totalSuccessfulPackets << ',' << totalPacketsEchoed << '\n';
    }
    std::cout << "RAW_OBSERVER beacons=" << raw_observer::beacons
              << " data=" << raw_observer::data << " wrong_slot=" << raw_observer::wrong
              << " page_mismatch=" << raw_observer::mismatch << std::endl;
    Simulator::Destroy();
	return trafficStarted && associated == config.Nsta && apAssociated == config.Nsta && !raw_observer::wrong && !raw_observer::mismatch ? 0 : 2;
}
