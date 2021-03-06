/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 *
 */

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
 #include "ns3/point-to-point-layout-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"

#define SSTR( x ) dynamic_cast< std::ostringstream & >( \
            ( std::ostringstream() << std::dec << x ) ).str()

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpScalableTest");













class MyApp : public Application 
{
public:

  /**
   * Default constructors.
   */
  MyApp ();
  virtual ~MyApp();

  /**
   * Setup the example application.
   */
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

/**
 * Default constructor.
 */
MyApp::MyApp ()
  : m_socket (0), 
    m_peer (), 
    m_packetSize (0), 
    m_nPackets (0), 
    m_dataRate (0), 
    m_sendEvent (), 
    m_running (false), 
    m_packetsSent (0)
{
}

/**
 * Default deconstructor.
 */
MyApp::~MyApp()
{
  m_socket = 0;
}

/**
 * Setup the application.
 * Parameters:
 * socket     Socket to send data to.
 * address    Address to send data to.
 * packetSize Size of the packets to send.
 * nPackets   Number of packets to send.
 * dataRate   Data rate used to determine when to send the packets.
 */
void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

/**
 * Start sending data to the address given in the Setup method.
 */
void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

/**
 * Stop sending data to the address given in the Setup method.
 */
void 
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

/**
 * Send a packet to the receiver.
 */
void 
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
  else
    {
      std::cout << "Done sending packets: " << m_packetsSent;
    }
  NS_LOG_UNCOND ("Packet sent");
}

/**
 * Schedule when the next packet will be sent.
 */
void 
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      NS_LOG_UNCOND("Send next packet at: " << tNext);
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}















int main (int argc, char *argv[])
{
  NS_LOG_UNCOND ("> Parse configuration");
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (512));
  // Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("1Mbps"));
  // Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpScalable::GetTypeId()));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize",  UintegerValue(1000));

  // cubic is the default congestion algorithm in Linux 2.6.26
  std::string tcpCong = "scalable";
  // this is the default error rate of our link, that is, the the probability of a single
  // byte being 'corrupted' during transfer.
  double errRate = 0.000001;
  // how long the sender should be running, in seconds.
  unsigned int runtime = 300;

  std::string backgroundRate = "10kbps";
  CommandLine cmd;
  // Here, we define additional command line options.
  // This allows a user to override the defaults set above from the command line.
  cmd.AddValue ("error-rate", "Error rate to apply to link", errRate);
  cmd.AddValue ("runtime", "How long the applications should send data (default 120 seconds)", runtime);
  cmd.AddValue ("bgRate", "Background traffic rate (default 10kbps)", backgroundRate);
  cmd.Parse (argc, argv);

  NS_LOG_UNCOND ("> Create nodes");
  NodeContainer internetNodes;

  const int internetNodesCount = 4;
  NS_LOG_UNCOND (">> Internet nodes in ring: " << internetNodesCount);
  internetNodes.Create (internetNodesCount);

  PointToPointHelper p2pBackbone;
  p2pBackbone.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("1Gbps")));
  p2pBackbone.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  InternetStackHelper internet;
  internet.Install (internetNodes);
  Ipv4InterfaceContainer ipv4InterfacesInternet[4];

  for (int i=0; i<internetNodesCount; i++) {
    int j = i + 1;
    if (j >= internetNodesCount) j = 0;
    NodeContainer twoNodes = NodeContainer (internetNodes.Get (i), internetNodes.Get (j));

    NetDeviceContainer p2pInterfaces = p2pBackbone.Install(twoNodes);
    // The default MTU of the p2p link would be 65535, which doesn't work
    // well with our default errRate (most packets would arrive corrupted).
    p2pInterfaces.Get (0)->SetMtu (1500);
    p2pInterfaces.Get (1)->SetMtu (1500);

    Ipv4AddressHelper ipv4;
    std::string baseIp = SSTR(i + 1) + std::string(".0.0.0");
    ipv4.SetBase (baseIp.c_str(), "255.255.255.0");
    ipv4InterfacesInternet[i] = ipv4.Assign (p2pInterfaces);
    NS_LOG_UNCOND (">>> Connect " << i << " and " << j << ": " <<
                    ipv4InterfacesInternet[i].GetAddress(0) << "@" << internetNodes.Get (i)->GetId() << " => " << ipv4InterfacesInternet[i].GetAddress(1) << "@" << internetNodes.Get (j)->GetId());
  }

  PointToPointHelper p2pInternetProvider;
  // create point-to-point link with a bandwidth of 6MBit/s and a large delay (0.5 seconds)
  p2pInternetProvider.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
  // p2pInternetProvider.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (6 * 1000 * 1000)));
  p2pInternetProvider.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  PointToPointHelper p2pEndpoint;
  // create point-to-point link with a bandwidth of 6MBit/s and a large delay (0.5 seconds)
  // p2pEndpoint.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (6 * 1000 * 1000)));
  p2pEndpoint.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
  p2pEndpoint.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (40)));

  const int nodesInStar = 3;
  PointToPointStarHelper star1 = PointToPointStarHelper(nodesInStar, p2pEndpoint);
  PointToPointStarHelper star2 = PointToPointStarHelper(nodesInStar, p2pEndpoint);
  PointToPointStarHelper starNetworks[2] = {star1, star2};

  Ipv4AddressHelper ipv4StarNetworks[2];
  Ipv4InterfaceContainer ipv4InterfacesStarNetworks[2];
  NetDeviceContainer starNetDeviceContainer[2];

  NS_LOG_UNCOND ("> Create networks");
  for (int i=0, starId=0; i<internetNodesCount; i+=2, starId++) {
    starNetworks[starId].InstallStack (internet);
    std::string baseIp = SSTR(i + 1) + std::string(".0.100.0");
    starNetworks[starId].AssignIpv4Addresses (Ipv4AddressHelper (baseIp.c_str(), "255.255.255.0"));

    NS_LOG_UNCOND (">> Star of " << nodesInStar << " nodes at " << baseIp << "/24");

    NodeContainer twoNodes = NodeContainer (internetNodes.Get (i), starNetworks[starId].GetHub ());
    starNetDeviceContainer[starId] = p2pInternetProvider.Install(twoNodes);
    baseIp = SSTR(i + 1) + std::string(".1.0.0");
    ipv4StarNetworks[starId].SetBase (baseIp.c_str(), "255.255.255.0");
    ipv4InterfacesStarNetworks[starId] = ipv4StarNetworks[starId].Assign (starNetDeviceContainer[starId]);
    NS_LOG_UNCOND (">>> Connect star hub " << ipv4InterfacesStarNetworks[starId].GetAddress(1) << " @" << starNetworks[starId].GetHub ()->GetId() <<
                   " to internet node " << ipv4InterfacesStarNetworks[starId].GetAddress(0) << " @" << internetNodes.Get (i)->GetId());
  }


  NodeContainer cloud1;
  cloud1.Create (1);
  NodeContainer cloud2;
  cloud2.Create (1);
  NodeContainer cloudNetworks[2] = {cloud1, cloud2};
  Ipv4InterfaceContainer ipv4InterfacesCloud[2];

  for (int i=1, cloudId=0; i<internetNodesCount; i+=2, cloudId++) {
    internet.Install (cloudNetworks[cloudId]);

    NS_LOG_UNCOND (">> Cloud");

    NodeContainer twoNodes = NodeContainer (internetNodes.Get (i), cloudNetworks[cloudId].Get (0));
    NetDeviceContainer p2pInterfaces = p2pInternetProvider.Install(twoNodes);
    Ipv4AddressHelper ipv4;
    std::string baseIp = SSTR(i + 1) + std::string(".1.0.0");
    ipv4.SetBase (baseIp.c_str(), "255.255.255.0");
    ipv4InterfacesCloud[cloudId] = ipv4.Assign (p2pInterfaces);
    NS_LOG_UNCOND (">>> Connect cloud hub " << ipv4InterfacesCloud[cloudId].GetAddress(1)  << " @" << cloudNetworks[cloudId].Get (0)->GetId() <<
                   " to internet node " << ipv4InterfacesCloud[cloudId].GetAddress(0) << " @" << internetNodes.Get (i)->GetId());
  }

  NS_LOG_UNCOND ("> Setup traffic");
  // p2pInterfaces.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em2));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t servPort = 8080;
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), servPort));

  //============================
  // ns3::Ptr<ns3::Node> receiverNode = internetNodes.Get(1);
  // Ipv4Address receiverAddr = ipv4InterfacesInternet[0].GetAddress(1);
  // ns3::Ptr<ns3::Node> receiverNode = internetNodes.Get(0);
  // Ipv4Address receiverAddr = ipv4InterfacesStarNetworks[0].GetAddress(0);
  // ns3::Ptr<ns3::Node> receiverNode = starNetworks[0].GetHub(); 
  // Ipv4Address receiverAddr = starNetworks[0].GetHubIpv4Address(0);
  ns3::Ptr<ns3::Node> receiverNode = starNetworks[1].GetSpokeNode(0);
  Ipv4Address receiverAddr = starNetworks[1].GetSpokeIpv4Address(0);

  ns3::Ptr<ns3::Node> senderNode = starNetworks[0].GetSpokeNode(0);
  Ipv4Address senderAddr = starNetworks[0].GetSpokeIpv4Address(0);


  // DoubleValue rate (errRate);
  // Ptr<RateErrorModel> em1 = 
  //   CreateObjectWithAttributes<RateErrorModel> ("RanVar", StringValue ("ns3::UniformRandomVariable[Min=0.0,Max=1.0]"), "ErrorRate", rate);
  // starNetDeviceContainer[1].Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em1));

  //============================

  ApplicationContainer sinkApp = sinkHelper.Install (receiverNode);
  sinkApp.Start (Seconds (0.0));
  // this makes sure that the receiver will run one minute longer than the sender applicaton.
  sinkApp.Stop (Seconds (runtime + 60.0));
  NS_LOG_UNCOND ("Receiver at " << receiverAddr << ":" << servPort << " @" << receiverNode->GetId());

  Address remoteAddress (InetSocketAddress (receiverAddr , servPort));



  ns3::Ptr<ns3::Node> bgReceiverNode = starNetworks[1].GetHub();//cloudNetworks[1].Get(0);
  Ipv4Address bgReceiverAddr = starNetworks[1].GetHubIpv4Address(0); //ipv4InterfacesCloud[1].GetAddress(0);
  // ns3::Ptr<ns3::Node> bgReceiverNode2 = starNetworks[1].GetSpokeNode(2);//cloudNetworks[1].Get(0);
  // Ipv4Address bgReceiverAddr2 = starNetworks[1].GetSpokeIpv4Address(2); //ipv4InterfacesCloud[1].GetAddress(0);
  // ns3::Ptr<ns3::Node> bgReceiverNode = cloudNetworks[1].Get(0);
  // Ipv4Address bgReceiverAddr = ipv4InterfacesCloud[1].GetAddress(1);

  // ns3::Ptr<ns3::Node> bgSenderNode = starNetworks[1].GetSpokeNode(2);
  ns3::Ptr<ns3::Node> bgSenderNode = cloudNetworks[0].Get(0);
  // ns3::Ptr<ns3::Node> bgSenderNode = internetNodes.Get(1);
  // Ipv4Address bgSenderAddr = starNetworks[0].GetSpokeIpv4Address(0);

  ApplicationContainer bgSinkApp = sinkHelper.Install (bgReceiverNode);
  // ApplicationContainer bgSinkApp2 = sinkHelper.Install (bgReceiverNode2);
  bgSinkApp.Start (Seconds (0.0));
  bgSinkApp.Stop (Seconds (runtime + 60.0));
  // bgSinkApp2.Start (Seconds (0.0));
  // bgSinkApp2.Stop (Seconds (runtime + 60.0));
  NS_LOG_UNCOND ("Receiver at " << bgReceiverAddr << ":" << servPort << " @" << bgReceiverNode->GetId());

  Address bgRemoteAddress (InetSocketAddress (bgReceiverAddr , servPort));
  // Address bgRemoteAddress2 (InetSocketAddress (bgReceiverAddr2 , servPort));















  TypeId tid = TypeId::LookupByName ("ns3::TcpScalable");
  Config::Set ("/NodeList/*/$ns3::TcpL4Protocol/SocketType", TypeIdValue (tid));
  // Config::Set ("/NodeList/*/$ns3::TcpSocketBase/SlowStartThreshold", UintegerValue(2621400));
  // Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (senderNode, /*tid*/ TcpSocketFactory::GetTypeId ());

  //-------------
  // OnOffHelper clientHelper ("ns3::TcpSocketFactory", remoteAddress);
  // clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  // clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  // ApplicationContainer clientApp = clientHelper.Install (senderNode);
  // clientApp.Start(Seconds(0.2));
  // clientApp.Stop(Seconds(runtime-2));
  //-------------
  // Ptr<MyApp> app = CreateObject<MyApp> ();
  // app->Setup (ns3TcpSocket, remoteAddress, 1000, 100000000, DataRate ("1Mbps"));
  // senderNode->AddApplication (app);
  // app->SetStartTime (Seconds (0.2));
  // app->SetStopTime (Seconds (60.0));
  //-------------

  BulkSendHelper clientHelper ("ns3::TcpSocketFactory", remoteAddress);
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (0));
  // OnOffHelper clientHelper ("ns3::TcpSocketFactory", remoteAddress);
  // clientHelper.SetConstantRate(DataRate ("900kbps"), 512);
  ApplicationContainer clientApp = clientHelper.Install (senderNode);
  clientApp.Start(Seconds(1));
  clientApp.Stop(Seconds(runtime-2));


  //noise

  // BulkSendHelper clientHelper ("ns3::TcpSocketFactory", bgRemoteAddress);
  // clientHelper.SetAttribute ("MaxBytes", UintegerValue (0));
  // ApplicationContainer clientApp = clientHelper.Install (bgSenderNode);
  // clientApp.Start(Seconds(1));
  // clientApp.Stop(Seconds(runtime-2));

// ==========
  OnOffHelper bgClientHelper ("ns3::TcpSocketFactory", bgRemoteAddress);
  bgClientHelper.SetConstantRate(DataRate (backgroundRate), 512);
  // bgClientHelper.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer bgClientApp = bgClientHelper.Install (bgSenderNode);
  bgClientApp.Start(Seconds(0));
  bgClientApp.Stop(Seconds(runtime));
// ==========

  // OnOffHelper bgClientHelper2 ("ns3::TcpSocketFactory", bgRemoteAddress2);
  // bgClientHelper2.SetConstantRate(DataRate (backgroundRate), 512);
  // // bgClientHelper2.SetAttribute ("MaxBytes", UintegerValue (0));
  // ApplicationContainer bgClientApp2 = bgClientHelper2.Install (bgSenderNode);
  // bgClientApp2.Start(Seconds(0));
  // bgClientApp2.Stop(Seconds(runtime));















  // OnOffHelper clientHelper ("ns3::TcpSocketFactory", remoteAddress);
  // clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  // clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  // ApplicationContainer clientApp = clientHelper.Install (senderNode);
  // clientApp.Start (Seconds (1.0));
  // clientApp.Stop (Seconds (runtime - 2.0));
  NS_LOG_UNCOND ("Sender at " << senderAddr << ":" << servPort << " @" << senderNode->GetId());
  // devices.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));

  p2pEndpoint.EnablePcapAll ("coursework");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.Install( starNetworks[0].GetSpokeNode(0) );

  // sinkApp.Start (Seconds (0.0));
  // // this makes sure that the receiver will run one minute longer than the sender applicaton.
  // sinkApp.Stop (Seconds (runtime + 60.0));

  // // This sets up two TCP flows, one from A -> B, one from B -> A.
  // for (int i = 0, j = 1; i < 2; j--, i++)
  //   {
  //     Address remoteAddress (InetSocketAddress (ipv4Interfaces.GetAddress (i), servPort));
  //     OnOffHelper clientHelper ("ns3::TcpSocketFactory", remoteAddress);
  //     clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  //     clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  //     ApplicationContainer clientApp = clientHelper.Install (n.Get (j));
  //     clientApp.Start (Seconds (1.0 + i));
  //     clientApp.Stop (Seconds (runtime + 1.0 + i));
  //   }

  // // This tells ns-3 to generate pcap traces.
  // p2p.EnablePcapAll ("tcp-nsc-lfn");

  NS_LOG_UNCOND ("> Simulation");

  Simulator::Stop (Seconds (900));
  Simulator::Run ();
  monitor->SerializeToXmlFile ("results.xml", true, true);
  Simulator::Destroy ();

  return 0;
}
