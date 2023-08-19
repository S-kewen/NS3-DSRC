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

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("WaveExample1");

struct Packetitem
{
  int fromDev = -1;
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

void expRecord(double syncTime)
{
  expStartTime = getTimeStamp();
  // cout << "exp start: " << expStartTime << " = " << syncTime << endl;
  double lastRrcordTime = 0;
  std::ofstream myfile;
  myfile.open(to_string(syncTime) + ".csv", std::ofstream::out | std::ofstream::app);
  myfile << "start"
         << "," << expStartTime << "\n";
  myfile.close();
  while (true)
  {
    myfile.open(to_string(syncTime) + ".csv", std::ofstream::out | std::ofstream::app);
    if (m_simTime != lastRrcordTime)
    {
      cout << m_simTime << "," << getTimeStamp() - expStartTime << endl;
      myfile << m_simTime << "," << getTimeStamp() - expStartTime << "\n";
      lastRrcordTime = m_simTime;
    }
    myfile.close();
  }
}

int parseMac(std::string mac)
{
  std::vector<string> macs = split(mac, ":");
  return stoi(macs[macs.size() - 1]) - 1;
}

// Note: this is a promiscuous trace for all packet reception. This is also on physical layer, so packets still have WifiMacHeader
void Rx(zmq::socket_t *pkgSendSocket, std::map<uint64_t, Packetitem> *umap, std::set<uint32_t> *receivedUids, std::string context, Ptr<const Packet> packet, uint16_t channelFreqMhz, WifiTxVector txVector, MpduInfo aMpdu, SignalNoiseDbm signalNoise)
{
  int receivemacStart = context.find("NodeList/") + 9;
  int receivemacEnd = context.find("/DeviceList");
  WifiMacHeader hdr;
  uint32_t uid = packet->GetUid();
  if (packet->PeekHeader(hdr))
  {
    stringstream ss;
    std::string addr1 = "";
    std::string addr2 = "";
    ss << hdr.GetAddr1();
    ss >> addr1;
    ss.clear();
    ss << hdr.GetAddr2();
    ss >> addr2;
    char str[1000];
    Packetitem pt = umap->operator[](uid);
    // if (pt.type > 0 && pt.receiverId == 1 && stoi(context.substr(receivemacStart, receivemacEnd - receivemacStart)) == 1)
    //   {
    //     std::cout << str << endl;
    //   }
    if (pt.type > 0 && receivedUids->find(uid) == receivedUids->end()) // [skewen]: not ad-hoc, needs to check receiverId
    {
      receivedUids->insert(uid);
      sprintf(str, "{\"type\":\"rx\",\"sendTime\":%lf,\"recvTime\":%lf,\"packetuid\":%lu,\"size\":%d,\"freq\":%d,\"mode\":\"%s\",\"signal\":%.4f,\"noise\":%.4f,\"destinationMac\":\"%s\",\"receiveMac\":\"%d\",\"sourceMac\":\"%d\",\"seqNo\":\"%d\",\"frame\":\"%d\",\"segment\":\"%d\",\"x\":%lf,\"y\":%lf,\"z\":%lf,\"rx\":%lf,\"ry\":%lf,\"rz\":%lf,\"id\":%d}", pt.sendT, Simulator::Now().GetSeconds(), uid, packet->GetSize(), channelFreqMhz, txVector.GetMode().GetUniqueName().c_str(), signalNoise.signal, signalNoise.noise, addr1.c_str(), stoi(context.substr(receivemacStart, receivemacEnd - receivemacStart)), parseMac(addr2.c_str()), hdr.GetSequenceNumber(), pt.frame, pt.segment, pt.x, pt.y, pt.z, pt.rx, pt.ry, pt.rz, pt.fromDev);
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

/*
 * This function works for ns-3.30 onwards. For previous version, remove the last parameter (the "WifiPhyRxfailureReason")
 */
void PhyRxDrop(zmq::socket_t *pkgSendSocket, std::map<uint64_t, Packetitem> *umap, std::string context, Ptr<const Packet> packet, ns3::WifiPhyRxfailureReason reason)
{
  int receivemacStart = context.find("NodeList/") + 9;
  int receivemacEnd = context.find("/DeviceList");
  WifiMacHeader hdr;
  if (packet->PeekHeader(hdr))
  {
    stringstream ss;
    std::string addr1 = "";
    std::string addr2 = "";
    ss << hdr.GetAddr1();
    ss >> addr1;
    ss.clear();
    ss << hdr.GetAddr2();
    ss >> addr2;
    char str[1000];
    Packetitem pt = umap->operator[](packet->GetUid());
    // std::cout <<"send time: "<<pt.sendT << " receive time: "<< Simulator::Now().GetSeconds()<<endl;
    sprintf(str, "{\"type\":\"phyRxDrop\",\"sendTime\":%lf,\"recvTime\":%lf,\"packetuid\":%lu,\"size\":%d,\"freq\":%d,\"mode\":\"%s\",\"signal\":%.4f,\"noise\":%.4f,\"destinationMac\":\"%s\",\"receiveMac\":\"%d\",\"sourceMac\":\"%d\",\"seqNo\":\"%d\",\"frame\":\"%d\",\"segment\":\"%d\",\"x\":%lf,\"y\":%lf,\"z\":%lf,\"rx\":%lf,\"ry\":%lf,\"rz\":%lf,\"id\":%d}", pt.sendT, Simulator::Now().GetSeconds(), packet->GetUid(), packet->GetSize(), 0, "-1", 0.0f, 0.0f, addr1.c_str(), stoi(context.substr(receivemacStart, receivemacEnd - receivemacStart)), parseMac(addr2.c_str()), hdr.GetSequenceNumber(), pt.frame, pt.segment, pt.x, pt.y, pt.z, pt.rx, pt.ry, pt.rz, pt.fromDev);

    std::string s = string(str);
    zmq::message_t message(s.size());
    memcpy((uint8_t *)(message.data()), s.data(), s.size());
    pkgSendSocket->send(message, zmq::send_flags::none);
    // std::cout<< "rx drop" << "\tsourceMac: " << parseMac(addr2.c_str()) << "\tframe: " << pt.frame << "\tsegment: " << pt.segment << endl;
    std::cout << str << endl;
  }
}

void PhyTxDrop(zmq::socket_t *pkgSendSocket, std::map<uint64_t, Packetitem> *umap, std::string context, Ptr<const Packet> packet)
{
  int receivemacStart = context.find("NodeList/") + 9;
  int receivemacEnd = context.find("/DeviceList");
  WifiMacHeader hdr;
  if (packet->PeekHeader(hdr))
  {
    stringstream ss;
    std::string addr1 = "";
    std::string addr2 = "";
    ss << hdr.GetAddr1();
    ss >> addr1;
    ss.clear();
    ss << hdr.GetAddr2();
    ss >> addr2;
    char str[1000];
    Packetitem pt = umap->operator[](packet->GetUid());
    // std::cout << "send time: " << pt.sendT << " receive time: " << Simulator::Now().GetSeconds() << endl;
    sprintf(str, "{\"type\":\"phyTxDrop\",\"sendTime\":%lf,\"recvTime\":%lf,\"packetuid\":%lu,\"size\":%d,\"freq\":%d,\"mode\":\"%s\",\"signal\":%.4f,\"noise\":%.4f,\"destinationMac\":\"%s\",\"receiveMac\":\"%d\",\"sourceMac\":\"%d\",\"seqNo\":\"%d\",\"frame\":\"%d\",\"segment\":\"%d\",\"x\":%lf,\"y\":%lf,\"z\":%lf,\"rx\":%lf,\"ry\":%lf,\"rz\":%lf,\"id\":%d}", pt.sendT, Simulator::Now().GetSeconds(), packet->GetUid(), packet->GetSize(), 0, "-1", 0.0f, 0.0f, addr1.c_str(), stoi(context.substr(receivemacStart, receivemacEnd - receivemacStart)), parseMac(addr2.c_str()), hdr.GetSequenceNumber(), pt.frame, pt.segment, pt.x, pt.y, pt.z, pt.rx, pt.ry, pt.rz, pt.fromDev);

    std::string s = string(str);
    zmq::message_t message(s.size());
    memcpy((uint8_t *)(message.data()), s.data(), s.size());
    pkgSendSocket->send(message, zmq::send_flags::none);
    // std::cout<< "rx drop" << "\tsourceMac: " << parseMac(addr2.c_str()) << "\tframe: " << pt.frame << "\tsegment: " << pt.segment << endl;
    std::cout << str << endl;
  }
}

void MacRxDrop(zmq::socket_t *pkgSendSocket, std::map<uint64_t, Packetitem> *umap, std::string context, Ptr<const Packet> packet)
{
  int receivemacStart = context.find("NodeList/") + 9;
  int receivemacEnd = context.find("/DeviceList");
  WifiMacHeader hdr;
  if (packet->PeekHeader(hdr))
  {
    stringstream ss;
    std::string addr1 = "";
    std::string addr2 = "";
    ss << hdr.GetAddr1();
    ss >> addr1;
    ss.clear();
    ss << hdr.GetAddr2();
    ss >> addr2;
    char str[1000];
    Packetitem pt = umap->operator[](packet->GetUid());
    // std::cout << "send time: " << pt.sendT << " receive time: " << Simulator::Now().GetSeconds() << endl;
    sprintf(str, "{\"type\":\"macRxDrop\",\"sendTime\":%lf,\"recvTime\":%lf,\"packetuid\":%lu,\"size\":%d,\"freq\":%d,\"mode\":\"%s\",\"signal\":%.4f,\"noise\":%.4f,\"destinationMac\":\"%s\",\"receiveMac\":\"%d\",\"sourceMac\":\"%d\",\"seqNo\":\"%d\",\"frame\":\"%d\",\"segment\":\"%d\",\"x\":%lf,\"y\":%lf,\"z\":%lf,\"rx\":%lf,\"ry\":%lf,\"rz\":%lf,\"id\":%d}", pt.sendT, Simulator::Now().GetSeconds(), packet->GetUid(), packet->GetSize(), 0, "-1", 0.0f, 0.0f, addr1.c_str(), stoi(context.substr(receivemacStart, receivemacEnd - receivemacStart)), parseMac(addr2.c_str()), hdr.GetSequenceNumber(), pt.frame, pt.segment, pt.x, pt.y, pt.z, pt.rx, pt.ry, pt.rz, pt.fromDev);

    std::string s = string(str);
    zmq::message_t message(s.size());
    memcpy((uint8_t *)(message.data()), s.data(), s.size());
    pkgSendSocket->send(message, zmq::send_flags::none);
    // std::cout<< "rx drop" << "\tsourceMac: " << parseMac(addr2.c_str()) << "\tframe: " << pt.frame << "\tsegment: " << pt.segment << endl;
    std::cout << str << endl;
  }
}

void MacTxDrop(zmq::socket_t *pkgSendSocket, std::map<uint64_t, Packetitem> *umap, std::string context, Ptr<const Packet> packet)
{
  int receivemacStart = context.find("NodeList/") + 9;
  int receivemacEnd = context.find("/DeviceList");
  WifiMacHeader hdr;
  if (packet->PeekHeader(hdr))
  {
    stringstream ss;
    std::string addr1 = "";
    std::string addr2 = "";
    ss << hdr.GetAddr1();
    ss >> addr1;
    ss.clear();
    ss << hdr.GetAddr2();
    ss >> addr2;
    char str[1000];
    Packetitem pt = umap->operator[](packet->GetUid());
    // std::cout << "send time: " << pt.sendT << " receive time: " << Simulator::Now().GetSeconds() << endl;
    sprintf(str, "{\"type\":\"macTxDrop\",\"sendTime\":%lf,\"recvTime\":%lf,\"packetuid\":%lu,\"size\":%d,\"freq\":%d,\"mode\":\"%s\",\"signal\":%.4f,\"noise\":%.4f,\"destinationMac\":\"%s\",\"receiveMac\":\"%d\",\"sourceMac\":\"%d\",\"seqNo\":\"%d\",\"frame\":\"%d\",\"segment\":\"%d\",\"x\":%lf,\"y\":%lf,\"z\":%lf,\"rx\":%lf,\"ry\":%lf,\"rz\":%lf,\"id\":%d}", pt.sendT, Simulator::Now().GetSeconds(), packet->GetUid(), packet->GetSize(), 0, "-1", 0.0f, 0.0f, addr1.c_str(), stoi(context.substr(receivemacStart, receivemacEnd - receivemacStart)), parseMac(addr2.c_str()), hdr.GetSequenceNumber(), pt.frame, pt.segment, pt.x, pt.y, pt.z, pt.rx, pt.ry, pt.rz, pt.fromDev);

    std::string s = string(str);
    zmq::message_t message(s.size());
    memcpy((uint8_t *)(message.data()), s.data(), s.size());
    pkgSendSocket->send(message, zmq::send_flags::none);
    // std::cout<< "rx drop" << "\tsourceMac: " << parseMac(addr2.c_str()) << "\tframe: " << pt.frame << "\tsegment: " << pt.segment << endl;
    std::cout << str << endl;
  }
}

// Fired when a packet is Enqueued in MAC
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
// Fired when a packet is Dequeued from MAC layer. A packet is dequeued before it is transmitted.
void DequeueTrace(std::map<uint64_t, Packetitem> *umap, std::string context, Ptr<const WifiMacQueueItem> item)
{
  Ptr<const Packet> p = item->GetPacket();
  Time queue_delay = Simulator::Now() - item->GetTimeStamp();

  Packetitem pt = umap->operator[](p->GetUid());
  pt.deQT = Simulator::Now();
  umap->operator[](p->GetUid()) = pt;
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

void zmq_recv_init()
{
  zmq::context_t context;
  zmq::socket_t zmqRecvSocket = zmq::socket_t(context, ZMQ_PULL);

  zmq::message_t message;
  Json::Reader reader;
  Json::Value json_object;
  while (true)
  {
    zmqRecvSocket.connect("tcp://127.0.0.1:5561");
    zmqRecvSocket.recv(message, zmq::recv_flags::dontwait);
    std::string json_data(static_cast<char *>(message.data()), message.size());
    if (reader.parse(json_data, json_object))
    {

      cout << json_object["type"].asString() << endl;
      if (json_object["type"].asString() == "init")
      {
        m_syncTime = json_object["syncTime"].asDouble();
        m_nodes = json_object["nodes"].asInt();
        if (json_object["break"].asInt() == 1)
        {
          break;
        }
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
        Ptr<NetDevice> d0 = ds.Get(json_object["id"].asInt());
        Ptr<WaveNetDevice> wd = DynamicCast<WaveNetDevice>(d0);

        double sendTime = json_object["timestamp"].asDouble();
        double scheduleTime = sendTime - m_simTime;

        if (scheduleTime < 0)
        {
          std::cout << "ERROR !!! " << sendTime << "-" << m_simTime << "=" << scheduleTime << endl;
        }
        Ptr<Packet> p = Create<Packet>(json_object["size"].asInt());
        Packetitem pt;
        pt.frame = json_object["frame"].asInt();
        pt.segment = json_object["segment"].asInt();
        pt.fromDev = json_object["id"].asInt();
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

        if (json_object["castMethod"].asString() == "broadcast")
        {
          TxInfo tx;
          tx.preamble = WIFI_PREAMBLE_LONG;
          tx.channelNumber = CCH;
          // We always need to set TxPower to something. Otherwise, default data rate would be used.
          tx.txPowerLevel = 3;
          tx.priority = 7;
          tx.dataRate = WifiMode(json_object["mode"].asString()); // OfdmRate27MbpsBW10MHz OfdmRate6MbpsBW10MHz
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

int main(int argc, char *argv[])
{
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

  // I prefer using WaveHelper to create WaveNetDevice
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
  wavePhy.EnablePcap("WaveTest", devices);

  std::cout << "Number of nodes: " << m_nodes << std::endl;
  // std::cout<<"timestamp"<<getTimeStamp()<<std::endl;
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
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WaveNetDevice/PhyEntities/*/MonitorSnifferRx", MakeBoundCallback(&Rx, &pkgSendSocket, &umap, &receivedUids));

  // Config::Connect("/NodeList/*/DeviceList/*/$ns3::WaveNetDevice/PhyEntities/*/MonitorSnifferTx", MakeBoundCallback(&MonitorSnifferTx, &pkgSendSocket, &umap));

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