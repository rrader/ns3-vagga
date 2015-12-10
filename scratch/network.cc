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

//
// Network topology
//
//           6Mb/s, 500ms
//       n0-----------------n1
//
// - a 'lossy' network with long delay
// - TCP flow from n0 to n1 and from n1 to n0
// - pcap traces generated as tcp-nsc-lfn-0-0.pcap and tcp-nsc-lfn-1-0.pcap
//  Usage (e.g.): ./waf --run 'tcp-nsc-lfn --TCP_CONGESTION=hybla --runtime=30'

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

#define SSTR( x ) dynamic_cast< std::ostringstream & >( \
            ( std::ostringstream() << std::dec << x ) ).str()

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpNscLfn");

int main (int argc, char *argv[])
{
  NS_LOG_UNCOND ("> Parse configuration");
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (4096));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("6Mbps"));

  // cubic is the default congestion algorithm in Linux 2.6.26
  std::string tcpCong = "scalable";
  // this is the default error rate of our link, that is, the the probability of a single
  // byte being 'corrupted' during transfer.
  double errRate = 0.000001;
  // how long the sender should be running, in seconds.
  unsigned int runtime = 120;
  // the name of the NSC stack library that should be used
  std::string nscStack = "liblinux2.6.26.so";

  CommandLine cmd;
  // Here, we define additional command line options.
  // This allows a user to override the defaults set above from the command line.
  cmd.AddValue ("TCP_CONGESTION", "Linux 2.6.26 Tcp Congestion control algorithm to use", tcpCong);
  cmd.AddValue ("error-rate", "Error rate to apply to link", errRate);
  cmd.AddValue ("runtime", "How long the applications should send data (default 120 seconds)", runtime);
  cmd.AddValue ("nscstack", "Set name of NSC stack (shared library) to use (default liblinux2.6.26.so)", nscStack);
  cmd.Parse (argc, argv);

  NS_LOG_UNCOND ("> Create nodes");
  NodeContainer internetNodes;
  
  const int internetNodesCount = 4;
  NS_LOG_UNCOND (">> Internet nodes in ring: " << internetNodesCount);
  internetNodes.Create (internetNodesCount);

  PointToPointHelper p2pBackbone;
  // create point-to-point link with a bandwidth of 6MBit/s and a large delay (0.5 seconds)
  // p2pBackbone.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (6 * 1000 * 1000)));
  // p2pBackbone.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (500)));

  InternetStackHelper internet;
  internet.SetTcp ("ns3::NscTcpL4Protocol","Library",StringValue (nscStack));
  internet.Install (internetNodes);

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
    Ipv4InterfaceContainer ipv4Interfaces = ipv4.Assign (p2pInterfaces);
    NS_LOG_UNCOND (">>> Connect " << i << " and " << j << ": " <<
                    ipv4Interfaces.GetAddress(0) << " => " << ipv4Interfaces.GetAddress(1));
  }

  NS_LOG_UNCOND ("> TCP Congestion control algorighm: " << tcpCong);
  Config::Set ("/NodeList/*/$ns3::Ns3NscStack<linux2.6.26>/net.ipv4.tcp_congestion_control", StringValue (tcpCong));

  PointToPointHelper p2pInternetProvider;
  // create point-to-point link with a bandwidth of 6MBit/s and a large delay (0.5 seconds)
  // p2pInternetProvider.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (6 * 1000 * 1000)));
  // p2pInternetProvider.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (500)));

  PointToPointHelper p2pEndpoint;
  // create point-to-point link with a bandwidth of 6MBit/s and a large delay (0.5 seconds)
  // p2pEndpoint.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (6 * 1000 * 1000)));
  // p2pEndpoint.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (500)));

  const int nodesInStar = 5;
  PointToPointStarHelper star1 = PointToPointStarHelper(nodesInStar, p2pEndpoint);
  PointToPointStarHelper star2 = PointToPointStarHelper(nodesInStar, p2pEndpoint);
  PointToPointStarHelper starNetworks[2] = {star1, star2};

  Ipv4AddressHelper ipv4StarNetworks[2];

  NS_LOG_UNCOND ("> Create networks");
  for (int i=0, starId=0; i<internetNodesCount; i+=2, starId++) {
    starNetworks[starId].InstallStack (internet);
    std::string baseIp = SSTR(i + 1) + std::string(".0.100.0");
    starNetworks[starId].AssignIpv4Addresses (Ipv4AddressHelper (baseIp.c_str(), "255.255.255.0"));

    NS_LOG_UNCOND (">> Star of " << nodesInStar << " nodes at " << baseIp << "/24");

    NodeContainer twoNodes = NodeContainer (internetNodes.Get (i), starNetworks[starId].GetHub ());
    NetDeviceContainer p2pInterfaces = p2pInternetProvider.Install(twoNodes);
    baseIp = SSTR(i + 1) + std::string(".1.0.0");
    ipv4StarNetworks[starId].SetBase (baseIp.c_str(), "255.255.255.0");
    Ipv4InterfaceContainer ipv4Interfaces = ipv4StarNetworks[starId].Assign (p2pInterfaces);
    NS_LOG_UNCOND (">>> Connect star hub " << ipv4Interfaces.GetAddress(1) <<
                   " to internet node " << ipv4Interfaces.GetAddress(0));
  }

  for (int i=1; i<internetNodesCount; i+=2) {
    NodeContainer cloud;
    cloud.Create (1);
    internet.Install (cloud);

    NS_LOG_UNCOND (">> Cloud");

    NodeContainer twoNodes = NodeContainer (internetNodes.Get (i), cloud.Get (0));
    NetDeviceContainer p2pInterfaces = p2pInternetProvider.Install(twoNodes);
    Ipv4AddressHelper ipv4;
    std::string baseIp = SSTR(i + 1) + std::string(".1.0.0");
    ipv4.SetBase (baseIp.c_str(), "255.255.255.0");
    Ipv4InterfaceContainer ipv4Interfaces = ipv4.Assign (p2pInterfaces);
    NS_LOG_UNCOND (">>> Connect cloud hub " << ipv4Interfaces.GetAddress(1) <<
                   " to internet node " << ipv4Interfaces.GetAddress(0));
  }

  NS_LOG_UNCOND ("> Setup traffic");
  // DoubleValue rate (errRate);
  // Ptr<RateErrorModel> em1 = 
  //   CreateObjectWithAttributes<RateErrorModel> ("RanVar", StringValue ("ns3::UniformRandomVariable[Min=0.0,Max=1.0]"), "ErrorRate", rate);
  // Ptr<RateErrorModel> em2 = 
  //   CreateObjectWithAttributes<RateErrorModel> ("RanVar", StringValue ("ns3::UniformRandomVariable[Min=0.0,Max=1.0]"), "ErrorRate", rate);

  // // This enables the specified errRate on both link endpoints.
  // p2pInterfaces.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em1));
  // p2pInterfaces.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em2));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t servPort = 8080;
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), servPort));

  ns3::Ptr<ns3::Node> receiverNode = starNetworks[1].GetSpokeNode(0);

  ApplicationContainer sinkApp = sinkHelper.Install (receiverNode);
  sinkApp.Start (Seconds (0.0));
  // this makes sure that the receiver will run one minute longer than the sender applicaton.
  sinkApp.Stop (Seconds (runtime + 60.0));
  NS_LOG_UNCOND ("Receiver at " << starNetworks[1].GetSpokeIpv4Address(3) << ":" << servPort);

  Address remoteAddress (InetSocketAddress (starNetworks[1].GetSpokeIpv4Address(0) , servPort));
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", remoteAddress);
  clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer clientApp = clientHelper.Install (starNetworks[0].GetSpokeNode(2));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (runtime - 2.0));
  NS_LOG_UNCOND ("Sender at " << starNetworks[0].GetSpokeIpv4Address(2) << ":" << servPort);

  p2pEndpoint.EnablePcapAll ("coursework-endpoints");
  p2pBackbone.EnablePcapAll ("coursework-internet");
  p2pInternetProvider.EnablePcapAll ("coursework-provider");
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
  Simulator::Destroy ();

  return 0;
}
