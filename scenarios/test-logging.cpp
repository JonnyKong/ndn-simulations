/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
    3  * This program is free software; you can redistribute it and/or modify
    4  * it under the terms of the GNU General Public License version 2 as
    5  * published by the Free Software Foundation;
    6  *
    7  * This program is distributed in the hope that it will be useful,
    8  * but WITHOUT ANY WARRANTY; without even the implied warranty of
    9  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   10  * GNU General Public License for more details.
   11  *
   12  * You should have received a copy of the GNU General Public License
   13  * along with this program; if not, write to the Free Software
   14  * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
   15  */
   
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/command-line.h"
#include "ns3/log.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;
 
namespace {
void
ReplacementTimePrinter (std::ostream &os)
{
  os << Simulator::Now ().GetSeconds () << "s";
}

void
ReplaceTimePrinter (void)
{
  std::cout << "Replacing time printer function after Simulator::Run ()" << std::endl;
  LogSetTimePrinter (&ReplacementTimePrinter);
}
}  // unnamed namespace

int main (int argc, char *argv[])
{
  bool replaceTimePrinter = true;
  std::string resolution = "Time::US";
  LogComponentEnable ("RandomVariableStream", LOG_LEVEL_ALL);
  LogComponentEnableAll (LOG_PREFIX_TIME);

  std::map<std::string, Time::Unit> resolutionMap = {{"Time::US", Time::US}, {"Time::NS", Time::NS}, {"Time::PS", Time::PS}, {"Time::FS", Time::FS}};
  CommandLine cmd;
  cmd.AddValue ("replaceTimePrinter", "replace time printing function", replaceTimePrinter);
  // cmd.AddValue ("resolution", "time resolution", resolution);
  cmd.Parse (argc, argv);

  auto search = resolutionMap.find (resolution);
  if (search != resolutionMap.end ())
  {
    Time::SetResolution (search->second);
  }
  Ptr<UniformRandomVariable> uniformRv = CreateObject<UniformRandomVariable> ();
  if (replaceTimePrinter)
  {
    Simulator::Schedule (Seconds (0), &ReplaceTimePrinter);
  }
  Simulator::Schedule (NanoSeconds (1), &UniformRandomVariable::SetAntithetic, uniformRv, false);
  Simulator::Schedule (NanoSeconds (123), &UniformRandomVariable::SetAntithetic, uniformRv, false);
  Simulator::Schedule (NanoSeconds (123456), &UniformRandomVariable::SetAntithetic, uniformRv, false);
  Simulator::Schedule (NanoSeconds (123456789), &UniformRandomVariable::SetAntithetic, uniformRv, false);

  Simulator::Run ();
  Simulator::Destroy ();
}