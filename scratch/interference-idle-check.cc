// Numerical unit regression: no nodes, channel, MAC, or network traffic.
#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/interference-helper.h"
#include <cmath>
#include <iomanip>
#include <iostream>
using namespace ns3;
static InterferenceHelper history, reference;
static WifiTxVector vector;
static Ptr<InterferenceHelper::Event> probe, cleanProbe;
static bool passed = true;
static void Compare () {
  auto a = history.CalculatePlcpPayloadSnrPer (probe);
  auto b = reference.CalculatePlcpPayloadSnrPer (cleanProbe);
  const bool ok = std::isfinite (a.snr) && a.snr > 0 && std::isfinite (a.per)
    && a.per >= 0 && a.per <= 1 && std::abs (a.snr / b.snr - 1) < 1e-10
    && std::abs (a.per - b.per) < 1e-10;
  std::cout << std::setprecision (17) << "history_snr=" << a.snr << " clean_snr=" << b.snr
            << " history_per=" << a.per << " clean_per=" << b.per << " pass=" << ok << std::endl;
  const bool activeNoisePreserved = Simulator::Now () < MilliSeconds (30) || (a.snr < 1.01 && a.per > 0);
  passed = passed && ok && activeNoisePreserved;
  history.NotifyRxEnd (); reference.NotifyRxEnd ();
}
static void ActiveNoise () {
  history.Add (128, vector, WIFI_PREAMBLE_S1G_SHORT, MilliSeconds (5), 1.9952623149688795);
  reference.Add (128, vector, WIFI_PREAMBLE_S1G_SHORT, MilliSeconds (5), 1.9952623149688795);
}
static void Probe () {
  probe = history.Add (128, vector, WIFI_PREAMBLE_S1G_SHORT, MilliSeconds (3), 1.9952623149688795);
  cleanProbe = reference.Add (128, vector, WIFI_PREAMBLE_S1G_SHORT, MilliSeconds (3), 1.9952623149688795);
  history.NotifyRxStart (); reference.NotifyRxStart ();
  Simulator::Schedule (MilliSeconds (3), &Compare);
}
int main () {
  vector = WifiTxVector (WifiPhy::GetOfdmRate650KbpsBW2MHz (), 0, 0, false, 1, 0, false);
  history.SetNoiseFigure (5.011872336272722); reference.SetNoiseFigure (5.011872336272722);
  history.SetErrorRateModel (CreateObject<NistErrorRateModel> ());
  reference.SetErrorRateModel (CreateObject<NistErrorRateModel> ());
  history.Add (128, vector, WIFI_PREAMBLE_S1G_SHORT, MilliSeconds (3), 1.9952623149688795);
  history.NotifyRxStart ();
  for (uint32_t i = 0; i < 8191; ++i)
    history.Add (128, vector, WIFI_PREAMBLE_S1G_SHORT, MicroSeconds (100 + (i * 37) % 1900),
                 std::pow (10.0, -8.0 + (i % 97) * 0.085));
  Simulator::Schedule (MilliSeconds (3), &InterferenceHelper::NotifyRxEnd, &history);
  Simulator::Schedule (MilliSeconds (10), &Probe);
  Simulator::Schedule (MilliSeconds (20), &Probe);
  Simulator::Schedule (MilliSeconds (29), &ActiveNoise);
  Simulator::Schedule (MilliSeconds (30), &Probe);
  Simulator::Run (); Simulator::Destroy ();
  std::cout << (passed ? "PASS" : "FAIL") << ": idle history must equal a clean receiver" << std::endl;
  return passed ? 0 : 2;
}
