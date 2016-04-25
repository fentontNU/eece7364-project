/*
 * Zachary Berry, Tyler Fenton, Devang Parekh, and Benjamin Tan
 * LTE Handover Research Project
 * EECE 7364 - Mobile and Wireless Networks
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LTEHandover");

int
main (int argc, char *argv[])
{
  int numberOfUes = 1;
  int numberOfEnbs = 4;
  int numBearersPerUe = 1;
  double distance = 100.0;
  double yForUe = 500.0;  
  double speed = 20;
  double simTime = (double)(numberOfEnbs + 1) * distance / speed;
  double enbTxPowerDbm = 46.0;

  Config::SetDefault("ns3::UdpClient::Interval", TimeValue(MilliSeconds(10)));
  Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(1000000));
  Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));

  /* Create the LTE Network */
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);
  lteHelper->SetSchedulerType("ns3::RrFfMacScheduler");

  /* Set algorithm parameters */
  /* We tweaked these for a number of our experiments */
  lteHelper->SetHandoverAlgorithmType("ns3::A2A4RsrqHandoverAlgorithm");
  lteHelper->SetHandoverAlgorithmAttribute("ServingCellThreshold", UintegerValue(30));
  lteHelper->SetHandoverAlgorithmAttribute("NeighbourCellOffset", UintegerValue(1));
  
  Ptr<Node> pgw = epcHelper->GetPgwNode ();

  /* Add the internet to the LTE Network*/
  NodeContainer remoteHostContainer;
  Ptr<Node> remoteHost;
  PointToPointHelper p2ph;
  InternetStackHelper internet;
  NetDeviceContainer internetDev;
  Ipv4AddressHelper ipv4Helper;
  Ipv4InterfaceContainer internetInterface;
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ipv4Address remoteHostAddr;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting;

  remoteHostContainer.Create(1);
  remoteHost = remoteHostContainer.Get(0);
  internet.Install(remoteHostContainer);

  /* Create channel parameters */
  p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
  p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
  p2ph.SetChannelAttribute("Delay", TimeValue(Seconds (0.010)));
  
  internetDev = p2ph.Install(pgw, remoteHost);
  ipv4Helper.SetBase("1.0.0.0", "255.0.0.0");
  internetInterface = ipv4Helper.Assign(internetDev);
  remoteHostAddr = internetInterface.GetAddress(1);
  remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  /* Create UE and enB */
  NodeContainer ueNodes;
  NodeContainer enbNodes;
  enbNodes.Create(numberOfEnbs);
  ueNodes.Create(numberOfUes);

  /* Position all eNB */
  /* We want them to be in a static position but spaced evenly */
  Ptr<ListPositionAllocator> enbPositionA = CreateObject<ListPositionAllocator> ();
  for (int i = 0; i < numberOfEnbs; i++) {
    Vector enbPosition(distance * (i + 1), distance, 0);
    enbPositionA->Add(enbPosition);
  }
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator(enbPositionA);
  enbMobility.Install(enbNodes);

  /* Have the UE move in a vector */
  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  ueMobility.Install(ueNodes);
  ueNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0, yForUe, 0));
  ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(speed, 0, 0));

  /* Install the network on our nodes */
  Config::SetDefault ("ns3::LteEnbPhy::TxPower", DoubleValue(enbTxPowerDbm));
  NetDeviceContainer enbLte = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLte = lteHelper->InstallUeDevice(ueNodes);

  internet.Install(ueNodes);
  Ipv4InterfaceContainer ueInterface;
  ueInterface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLte));
  for (uint i = 0; i < ueNodes.GetN(); i++) {
    Ptr<Node> ueNode = ueNodes.Get(i);
    Ptr<Ipv4StaticRouting> ueRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4> ());
    ueRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
  }

  for (int i = 0; i < numberOfUes; i++) {
    lteHelper->Attach (ueLte.Get(i), enbLte.Get(0));
  }

  int dlPort = 80000;
  int ulPort = 80001;

  Ptr<Node> ue = ueNodes.Get(0);
  Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
  ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

  /* Start sending traffic */
  for (int b = 0; b < numBearersPerUe; b++){
    ++dlPort;
    ++ulPort;

    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    UdpClientHelper dlClientHelper(ueInterface.GetAddress(0), dlPort);
    clientApps.Add (dlClientHelper.Install(remoteHost));
    PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory",
                                        InetSocketAddress(Ipv4Address::GetAny (), dlPort));
    serverApps.Add(dlPacketSinkHelper.Install(ue));
    
    UdpClientHelper ulClientHelper(remoteHostAddr, ulPort);
    clientApps.Add(ulClientHelper.Install (ue));
    PacketSinkHelper ulPacketSinkHelper("ns3::UdpSocketFactory",
                                        InetSocketAddress(Ipv4Address::GetAny (), ulPort));
    serverApps.Add (ulPacketSinkHelper.Install(remoteHost));
    
    Ptr<EpcTft> tft = Create<EpcTft>();
    EpcTft::PacketFilter dlPacketFilter;
    dlPacketFilter.localPortStart = dlPort;
    dlPacketFilter.localPortEnd = dlPort;
    tft->Add(dlPacketFilter);
    
    EpcTft::PacketFilter ulPacketFilter;
    ulPacketFilter.remotePortStart = ulPort;
    ulPacketFilter.remotePortEnd = ulPort;
    tft->Add(ulPacketFilter);
    
    EpsBearer bearer (EpsBearer::NGBR_VIDEO_TCP_DEFAULT);
    lteHelper->ActivateDedicatedEpsBearer(ueLte.Get(0), bearer, tft);

    Time start = Seconds(0); 
    serverApps.Start(start);
    clientApps.Start(start);
  }

  // Add X2 inteface
  lteHelper->AddX2Interface(enbNodes);

  /* Dump all trace files */
  p2ph.EnablePcapAll("lte-handover");
  lteHelper->EnablePhyTraces();
  lteHelper->EnableMacTraces();
  lteHelper->EnableRlcTraces();
  lteHelper->EnablePdcpTraces();
  Ptr<RadioBearerStatsCalculator> rlcStats = lteHelper->GetRlcStats ();
  rlcStats->SetAttribute ("EpochDuration", TimeValue (Seconds (1.0)));
  Ptr<RadioBearerStatsCalculator> pdcpStats = lteHelper->GetPdcpStats ();
  pdcpStats->SetAttribute ("EpochDuration", TimeValue (Seconds (1.0)));

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}
