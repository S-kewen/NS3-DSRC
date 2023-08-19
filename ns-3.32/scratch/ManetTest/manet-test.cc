#include "ns3/wave-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/core-module.h"
#include "custom-mobility-model.h"
#include "zmq.hpp"
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <thread>
#include <json/json.h>
#include <signal.h>
#include <chrono>
#include <time.h>
#include "ns3/seq-ts-header.h"
#include <sstream>
#include "ns3/llc-snap-header.h"
#include <map>
#include <unistd.h>
#include <string.h>
#include <vector>

#include "ns3/aodv-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/v4ping-helper.h"
#include "ns3/position-allocator.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/applications-module.h"
#include "ns3/v4ping.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/udp-server.h"
#include <cmath>
#include "ns3/packet.h"

using namespace std;
// For colorful console printing
/*
 * Usage example :
 *    std::cout << BOLD_CODE << "some bold text << END_CODE << std::endl;
 *
 *    std::cout << YELLOW_CODE << BOLD_CODE << "some bold yellow text << END_CODE << std::endl;
 *
 */
#define YELLOW_CODE "\033[33m"
#define TEAL_CODE "\033[36m"
#define BOLD_CODE "\033[1m"
#define END_CODE "\033[0m"
#define PROTOCOL_UDP 17
#define PROTOCOL_TCP 6

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("WaveExample1");

struct Packetitem
{
  int fromDev = -1;
  int toDev = -1;
  int receiverId = -1;
  int frame = -1;
  int segment = -1;
  double sendT = -1.0;
  ns3::Time deQT;
  double recvT = -1;
  double x = -1;
  double y = -1;
  double z = -1;
  double rx = -1;
  double ry = -1;
  double rz = -1;
  int type = -1;
};

NodeContainer gContainer;
NetDeviceContainer devices;

double m_simTime = 0;

double m_syncTime = 0.5;

uint32_t m_nodes = 0;

std::time_t expStartTime = 0;

vector<string> split(const string &str, const string &delim)
{
  vector<string> res;
  if ("" == str)
    return res;
  char *strs = new char[str.length() + 1];
  strcpy(strs, str.c_str());

  char *d = new char[delim.length() + 1];
  strcpy(d, delim.c_str());

  char *p = strtok(strs, d);
  while (p)
  {
    string s = p;
    res.push_back(s);
    p = strtok(NULL, d);
  }

  return res;
}

std::time_t getTimeStamp()
{
  std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
  std::time_t timestamp = tp.time_since_epoch().count();
  return timestamp;
}

int parseMac(std::string mac)
{
  std::vector<string> macs = split(mac, ":");
  return stoi(macs[macs.size() - 1]) - 1;
}

void Rx(zmq::socket_t *pkgSendSocket, std::map<uint64_t, Packetitem> *umap, std::set<uint32_t> *receivedUids, std::string context, Ptr<const Packet> packet, uint16_t channelFreqMhz, WifiTxVector txVector, MpduInfo aMpdu, SignalNoiseDbm signalNoise)
{
  int receivemacStart = context.find("NodeList/") + 9;
  int receivemacEnd = context.find("/DeviceList");
  int receiveMac = stoi(context.substr(receivemacStart, receivemacEnd - receivemacStart));
  WifiMacHeader hdr;
  uint32_t uid = packet->GetUid();
  if (packet->PeekHeader(hdr))
  {
    stringstream ss;
    std::string addr1 = "";
    std::string addr2 = "";
    std::string addr3 = "";
    std::string addr4 = "";
    ss << hdr.GetAddr1();
    ss >> addr1;
    ss.clear();
    ss << hdr.GetAddr2();
    ss >> addr2;
    ss.clear();
    ss << hdr.GetAddr3();
    ss >> addr3;
    ss.clear();
    ss << hdr.GetAddr4();
    ss >> addr4;
    char str[1000];
    Packetitem pt = umap->operator[](uid);

    // std::cout << "Rx: " << context << "\t" << addr1 << "\t" << addr2 << "\t" << addr3 << "\t" << addr4 << std::endl;

    if (pt.type > 0 && receivedUids->find(uid) == receivedUids->end()) // [skewen]: not ad-hoc, needs to check receiverId
    {
      receivedUids->insert(uid);
      sprintf(str, "{\"type\":\"rx\",\"sendTime\":%lf,\"recvTime\":%lf,\"packetuid\":%lu,\"size\":%d,\"freq\":%d,\"mode\":\"%s\",\"signal\":%.4f,\"noise\":%.4f,\"destinationMac\":\"%s\",\"receiveMac\":\"%d\",\"sourceMac\":\"%d\",\"seqNo\":\"%d\",\"frame\":\"%d\",\"segment\":\"%d\",\"x\":%lf,\"y\":%lf,\"z\":%lf,\"rx\":%lf,\"ry\":%lf,\"rz\":%lf,\"id\":%d}", pt.sendT, Simulator::Now().GetSeconds(), uid, packet->GetSize(), channelFreqMhz, txVector.GetMode().GetUniqueName().c_str(), signalNoise.signal, signalNoise.noise, addr1.c_str(), pt.toDev, parseMac(addr2.c_str()), hdr.GetSequenceNumber(), pt.frame, pt.segment, pt.x, pt.y, pt.z, pt.rx, pt.ry, pt.rz, pt.fromDev);
      // sourceMac start by 0
      // receivemac start by 0
      std::string s = string(str);
      zmq::message_t message(s.size());
      memcpy((uint8_t *)(message.data()), s.data(), s.size());
      pkgSendSocket->send(message, zmq::send_flags::none);
      // if (pt.receiverId == 1)
      // {
      //   std::cout << str << endl;
      // }
      std::cout << str << endl;
    }
  }
}

void zmq_update_position(zmq::socket_t *zmqRecvSocket)
{
  zmq::message_t message;
  Json::Reader reader;
  Json::Value json_object;
  while (true)
  {
    zmqRecvSocket->recv(message, zmq::recv_flags::dontwait);
    std::string json_data(static_cast<char *>(message.data()), message.size());
    if (reader.parse(json_data, json_object))
    {
      if (json_object["type"].asString() == "updateStatus" && json_object["status"].asInt() == 0)
      {
        break;
      }
      else if (json_object["type"].asString() == "setPosition")
      {
        Ptr<CustomMobilityModel> wnd = DynamicCast<CustomMobilityModel>(gContainer.Get(json_object["id"].asInt())->GetObject<MobilityModel>());
        double time = json_object["timestamp"].asDouble() - m_simTime;
        if (time < 0)
        {
          std::cout << "error!!!!" << json_object["timestamp"].asDouble() << "-" << m_simTime << "=" << time << endl;
        }
        Simulator::Schedule(Seconds(time), &ConstantPositionMobilityModel::SetPosition, wnd, Vector(Vector(json_object["x"].asDouble(), json_object["y"].asDouble(), json_object["z"].asDouble())));
      }
    }
  }
}

void zmq_simulation(NetDeviceContainer ds, zmq::socket_t *zmqRecvSocket, std::map<uint64_t, Packetitem> *umap)
{

  zmq::message_t message;
  Json::Reader reader;
  Json::Value json_object;
  uint16_t protocol = 0x88dc;
  while (true)
  {
    zmqRecvSocket->recv(message, zmq::recv_flags::none);
    std::string json_data(static_cast<char *>(message.data()), message.size());
    if (reader.parse(json_data, json_object))
    {
      if (json_object["type"].asString() == "updateStatus" && json_object["status"].asInt() == 0)
      {
        break;
      }
      else if (json_object["type"].asString() == "pushEvent")
      {
        double sendTime = json_object["timestamp"].asDouble();
        double scheduleTime = sendTime - m_simTime;

        if (scheduleTime < 0)
        {
          std::cout << "ERROR !!! " << sendTime << "-" << m_simTime << "=" << scheduleTime << endl;
        }

        Ptr<NetDevice> d0 = ds.Get(json_object["id"].asInt());
        Ptr<WaveNetDevice> wd = DynamicCast<WaveNetDevice>(d0);

        Ptr<Packet> p = Create<Packet>(json_object["size"].asInt());
        Ipv4Address receiverAddress = gContainer.Get(json_object["receiverId"].asInt())->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();

        Ipv4Header ipv4Header;
        ipv4Header.SetSource(gContainer.Get(json_object["id"].asInt())->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal());
        ipv4Header.SetDestination(gContainer.Get(json_object["receiverId"].asInt())->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal());
        ipv4Header.SetProtocol(PROTOCOL_UDP);
        ipv4Header.SetPayloadSize(json_object["size"].asInt());

        UdpHeader udpHeader;
        udpHeader.SetSourcePort(json_object["id"].asInt());
        udpHeader.SetDestinationPort(8888);

        p->AddHeader(udpHeader);
        p->AddHeader(ipv4Header);

        // TimestampTag txTime;
        // txTime.SetTimestamp(Simulator::Now());
        // p->AddPacketTag(txTime);

        Packetitem pt;
        pt.frame = json_object["frame"].asInt();
        pt.segment = json_object["segment"].asInt();
        pt.fromDev = json_object["id"].asInt();
        pt.toDev = json_object["receiverId"].asInt();
        pt.receiverId = json_object["receiverId"].asInt();
        pt.x = json_object["x"].asDouble();
        pt.y = json_object["y"].asDouble();
        pt.z = json_object["z"].asDouble();
        pt.rx = json_object["rx"].asDouble();
        pt.ry = json_object["ry"].asDouble();
        pt.rz = json_object["rz"].asDouble();
        pt.sendT = sendTime;
        pt.type = 1;
        umap->operator[](p->GetUid()) = pt;
        // std::cout << "send packet " << p->GetUid() << " from " << json_object["id"].asInt() << " to " << json_object["receiverId"].asInt() << std::endl;

        if (json_object["castMethod"].asString() == "broadcast")
        {
          TxInfo tx;
          tx.preamble = WIFI_PREAMBLE_LONG;
          tx.channelNumber = CCH;
          // We always need to set TxPower to something. Otherwise, default data rate would be used.
          tx.txPowerLevel = 3;
          tx.priority = 7;
          Mac48Address brocast_mac = Mac48Address::GetBroadcast();
          Simulator::Schedule(Seconds(scheduleTime), &WaveNetDevice::SendX, wd, p, brocast_mac, protocol, tx);
        }
        else if (json_object["castMethod"].asString() == "unicast")
        {
          Ptr<WaveNetDevice> d2 = DynamicCast<WaveNetDevice>(ds.Get(json_object["receiverId"].asInt()));
          Mac48Address unicast_mac = Mac48Address::ConvertFrom(d2->GetAddress());
          TxInfo tx_u;
          Simulator::Schedule(Seconds(scheduleTime), &WaveNetDevice::SendX, wd, p, unicast_mac, protocol, tx_u);
        }
      }
    }
  }
}

void takeTurn(NetDeviceContainer devices, zmq::socket_t *pkgRecvSocket, zmq::socket_t *posRecvSocket, zmq::socket_t *rsmSendSocket, std::map<uint64_t, Packetitem> *umap, double syncTime)
{
  std::cout << "start receive zmq_simulation" << std::endl;
  zmq_simulation(devices, pkgRecvSocket, umap);
  std::cout << "start receive zmq_update_position" << std::endl;
  zmq_update_position(posRecvSocket);
  m_simTime += syncTime;

  char str[1000];
  sprintf(str, "{\"type\":\"resume\",\"timestamp\":%lf,\"status\":1}", Simulator::Now().GetSeconds());
  std::string s = string(str);
  zmq::message_t message(s.size());
  memcpy((uint8_t *)(message.data()), s.data(), s.size());
  rsmSendSocket->send(message, zmq::send_flags::none);
  Simulator::Schedule(Seconds(syncTime), &takeTurn, devices, pkgRecvSocket, posRecvSocket, rsmSendSocket, umap, syncTime);
  std::cout << "m_simTime: " << m_simTime << std::endl;
}

void EnqueueTrace(std::string context, Ptr<const WifiMacQueueItem> item)
{
  // std::cout << TEAL_CODE << "A Packet was enqueued : " << context <<"timexxxxx:"<<item->GetTimeStamp()<< END_CODE << std::endl;

  Ptr<const Packet> p = item->GetPacket();
  // testheader hdr;
  // if (p->PeekHeader(hdr))
  // {
  //   std::cout << (Time)(item->GetTimeStamp()) << "xxxxxxxxxxxxxxxxxx" <<endl;
  //   hdr.SetTs((Time)(item->GetTimeStamp()));
  //   std::cout <<hdr.GetTs ().GetSeconds ();
  // }
  /*
   * Do something with the packet, like attach a tag. ns3 automatically attaches a timestamp for enqueued packets;
   */
}

void DequeueTrace(std::map<uint64_t, Packetitem> *umap, std::string context, Ptr<const WifiMacQueueItem> item)
{
  Ptr<const Packet> p = item->GetPacket();
  Time queue_delay = Simulator::Now() - item->GetTimeStamp();

  Packetitem pt = umap->operator[](p->GetUid());
  pt.deQT = Simulator::Now();
  umap->operator[](p->GetUid()) = pt;
}

int main(int argc, char *argv[])
{
  double txrange = 120;
  uint32_t serverId = 1;

  CommandLine cmd;
  cmd.AddValue("nodes", "nodes number", m_nodes);
  cmd.AddValue("syncTime", "sync time", m_syncTime);
  cmd.Parse(argc, argv);

  // std::cout << "waiting for init message...." << std::endl;
  // zmq_recv_init();
  // std::cout << "loading finished!!!" << std::endl;

  // m_nodes = 149;
  // m_syncTime = 0.5;

  std::cout << "m_nodes: " << m_nodes << std::endl;
  std::cout << "m_syncTime: " << m_syncTime << std::endl;

  if (m_nodes <= 0)
  {
    std::cout << "Nodes parms error !!!" << std::endl;
    return 0;
  }
  if (m_syncTime <= 0)
  {
    std::cout << "SyncTime Parms error !!!" << std::endl;
    return 0;
  }

  gContainer.Create(m_nodes);
  MobilityHelper mob;
  mob.SetMobilityModel("ns3::CustomMobilityModel");
  mob.Install(gContainer);

  // Initial positions
  //  Ptr<CustomMobilityModel> m0 = DynamicCast<CustomMobilityModel>(nodes.Get(0)->GetObject<MobilityModel>());
  //  change node position
  //  m0->SetPosition (Vector (50,10,0));

  // Nodes MUST have some sort of mobility because that's needed to compute the received signal strength
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  for (int i = 0; i < m_nodes; i++)
  {
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  }

  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(gContainer);

  YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
  YansWavePhyHelper wavePhy = YansWavePhyHelper::Default();
  wavePhy.SetChannel(waveChannel.Create());
  wavePhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

  /*
   * If you create applications that control TxPower, define the low & high end of TxPower.
   * This is done by using 'TxInfo' as shown below.
   * 33 dBm is the highest allowed for non-government use (as per 802.11-2016 standard, page 3271
   * 44.8 dBm is for government use.
   *
   * Setting them to the same value is the easy way to go.
   * I can instead set TxPowerStart to a value lower than 33, but then I need to set the number of levels for each PHY
   */
  wavePhy.Set("TxPowerStart", DoubleValue(8));
  wavePhy.Set("TxPowerEnd", DoubleValue(33));
  // wavePhy.Set("ChannelWidth", UintegerValue (1));

  QosWaveMacHelper waveMac = QosWaveMacHelper::Default();
  WaveHelper waveHelper = WaveHelper::Default();

  waveHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                                     "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"),
                                     "NonUnicastMode", StringValue("OfdmRate6MbpsBW10MHz"));
  devices = waveHelper.Install(wavePhy, waveMac, gContainer);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(aodv); // has effect on the next Install ()
  stack.Install(gContainer);
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign(devices);

  std::cout << "Number of nodes: " << m_nodes << std::endl;
  std::cout << "syncTime: " << m_syncTime << endl;

  zmq::context_t context;
  zmq::socket_t pkgRecvSocket = zmq::socket_t(context, ZMQ_PULL);
  pkgRecvSocket.connect("tcp://127.0.0.1:5558"); // recv packet event

  zmq::socket_t posRecvSocket = zmq::socket_t(context, ZMQ_PULL);
  posRecvSocket.connect("tcp://127.0.0.1:5557"); // recv position

  zmq::socket_t pkgSendSocket = zmq::socket_t(context, ZMQ_PUSH);
  pkgSendSocket.bind("tcp://127.0.0.1:5559"); // send simulation result

  zmq::socket_t rsmSendSocket = zmq::socket_t(context, ZMQ_PUSH);
  rsmSendSocket.bind("tcp://127.0.0.1:5560"); // send resume signal

  std::map<uint64_t, Packetitem> umap;
  std::set<uint32_t> receivedUids;

  Simulator::ScheduleNow(&takeTurn, devices, &pkgRecvSocket, &posRecvSocket, &rsmSendSocket, &umap, m_syncTime);
  // Config::Connect("/NodeList/*/DeviceList/*/Phy/State/RxOk", MakeBoundCallback(&RxOk, &pkgSendSocket, &umap, &receivedUids));
  // Config::Connect("/NodeList/*/DeviceList/*/Mac/MacRx", MakeBoundCallback(&MacRx, &pkgSendSocket, &umap, &receivedUids));

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WaveNetDevice/PhyEntities/*/MonitorSnifferRx", MakeBoundCallback(&Rx, &pkgSendSocket, &umap, &receivedUids));

  // Set the number of power levels.
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WaveNetDevice/PhyEntities/*/TxPowerLevels", ns3::UintegerValue(7));

  /*
   * What if some packets were dropped due to collision, or whatever? We use this trace to fire RxDrop function
   */

  // Config::Connect("/NodeList/*/DeviceList/*/$ns3::WaveNetDevice/PhyEntities/*/PhyRxDrop", MakeBoundCallback(&PhyRxDrop, &pkgSendSocket, &umap));

  // Config::Connect("/NodeList/*/DeviceList/*/$ns3::WaveNetDevice/PhyEntities/*/PhyTxDrop", MakeBoundCallback(&PhyTxDrop, &pkgSendSocket, &umap));

  // Config::Connect("/NodeList/*/DeviceList/*/$ns3::WaveNetDevice/MacEntities/*/MacRxDrop", MakeBoundCallback(&MacRxDrop, &pkgSendSocket, &umap));

  // Config::Connect("/NodeList/*/DeviceList/*/$ns3::WaveNetDevice/MacEntities/*/MacTxDrop", MakeBoundCallback(&MacTxDrop, &pkgSendSocket, &umap));

  /*
   * We can also trace some MAC layer details
   */
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WaveNetDevice/MacEntities/*/$ns3::OcbWifiMac/*/Queue/Enqueue", MakeCallback(&EnqueueTrace));

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WaveNetDevice/MacEntities/*/$ns3::OcbWifiMac/*/Queue/Dequeue", MakeBoundCallback(&DequeueTrace, &umap));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}