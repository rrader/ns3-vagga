/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Adrian Sai-wah Tam
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
 *
 * Author: Adrian Sai-wah Tam <adrian.sw.tam@gmail.com>
 */

#define NS_LOG_APPEND_CONTEXT \
  if (m_node) { std::clog << Simulator::Now ().GetSeconds () << " [node " << m_node->GetId () << "] "; }

#include "tcp-scalable.h"
#include "ns3/log.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/simulator.h"
#include "ns3/abort.h"
#include "ns3/node.h"


#define TCP_SCALABLE_AI_CNT     50U
#define TCP_SCALABLE_MD_SCALE   3


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpScalable");

NS_OBJECT_ENSURE_REGISTERED (TcpScalable);

TypeId
TcpScalable::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpScalable")
    .SetParent<TcpSocketBase> ()
    .SetGroupName ("Internet")
    .AddConstructor<TcpScalable> ()
  ;
  return tid;
}

TcpScalable::TcpScalable (void) : m_successfulAckCount (0)
{
  NS_LOG_FUNCTION (this);
}

TcpScalable::TcpScalable (const TcpScalable& sock)
  : TcpSocketBase (sock),
    m_successfulAckCount (0)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("Invoked the copy constructor");
}

TcpScalable::~TcpScalable (void)
{
  // m_ssThresh = 0x7fffffff; // from tcp.h
}

void TcpScalable::SetInitialSSThresh (uint32_t threshold) {
  // m_initialSsThresh = 0x7fffffff;
  TcpSocketBase::SetInitialSSThresh(threshold);
}

Ptr<TcpSocketBase>
TcpScalable::Fork (void)
{
  return CopyObject<TcpScalable> (this);
}

/* New ACK (up to seqnum seq) received. Increase cwnd and call TcpSocketBase::NewAck() */
void
TcpScalable::NewAck (SequenceNumber32 const& seq)
{
  // m_successfulAckCount += 1;
  // // NS_LOG_FUNCTION (this << seq);
  // NS_LOG_UNCOND ("TcpScalable received ACK for seq " << seq <<
  //               " cwnd " << m_cWnd <<
  //               " ssthresh " << m_ssThresh);
  // if (m_cWnd < m_ssThresh)
  //   { // Slow start mode
  //     m_cWnd = std::min(m_cWnd + m_segmentSize, m_ssThresh);
  //     NS_LOG_UNCOND ("In SlowStart, updated to cwnd " << m_cWnd << " ssthresh " << m_ssThresh);
  //   }
  // else
  //   { // Congestion avoidance mode
  //     // To increase cwnd for one segSize per 100 ACK
  //     if (m_successfulAckCount >= 100) {
  //       m_cWnd += m_segmentSize;
  //       m_successfulAckCount = 0;
  //       NS_LOG_UNCOND ("In CongAvoid, updated to cwnd " << m_cWnd << " ssthresh " << m_ssThresh);
  //     }
  //   }
  // TcpSocketBase::NewAck (seq);           // Complete newAck processing
  m_successfulAckCount += 1;
  NS_LOG_UNCOND ("Received ACK for seq " << seq <<
                " cwnd " << m_cWnd <<
                " ssthresh " << m_ssThresh);
  if (m_cWnd < m_ssThresh)
    { // Slow start mode, add one segSize to cWnd. Default m_ssThresh is 65535. (RFC2001, sec.1)
      m_cWnd += m_segmentSize;
      NS_LOG_UNCOND ("In SlowStart, updated to cwnd " << m_cWnd << " ssthresh " << m_ssThresh);
    }
  else
    { // Congestion avoidance mode, increase by (segSize*segSize)/cwnd. (RFC2581, sec.3.1)
      // To increase cwnd for one segSize per RTT, it should be (ackBytes*segSize)/cwnd
      // double adder = static_cast<double> (m_segmentSize * m_segmentSize) / m_cWnd.Get ();
      // adder = std::max (1.0, adder);
      // m_cWnd += static_cast<uint32_t> (adder);
      if (m_successfulAckCount >= 100) {
        m_cWnd += m_segmentSize;
        m_successfulAckCount = 0;
        NS_LOG_UNCOND ("In CongAvoid, updated to cwnd " << m_cWnd << " ssthresh " << m_ssThresh);
      }
      NS_LOG_UNCOND ("In CongAvoid, updated to cwnd " << m_cWnd << " ssthresh " << m_ssThresh);
    }
  TcpSocketBase::NewAck (seq);           // Complete newAck processing

  NS_LOG_UNCOND ("CSV," << this << "," << Simulator::Now ().GetSeconds () << "," << m_cWnd << "," << m_ssThresh);
}

/* Cut down ssthresh upon triple dupack */
void
TcpScalable::DupAck (const TcpHeader& t, uint32_t count)
{
  m_successfulAckCount = 0; // reset
  // m_ssThresh = std::max (static_cast<unsigned> (m_cWnd - (m_cWnd / 8)), m_segmentSize * 2);  // ssthresh*7/8
  // // m_cWnd = std::max (static_cast<unsigned> (m_cWnd - (m_cWnd / 8)), m_segmentSize * 2);  // ssthresh*7/8
  // // m_cWnd = m_segmentSize; // Run slow start again
  // // m_nextTxSequence = m_txBuffer->HeadSequence (); // Restart from highest Ack
  // DoRetransmit ();
  // NS_LOG_UNCOND ("DupAck " << count << ". m_ssThresh updated to " << m_ssThresh);
  // // if (count == m_retxThresh)
  // //   { // triple duplicate ack triggers fast retransmit (RFC2001, sec.3)
  // //     NS_LOG_UNCOND ("Triple Dup Ack: old ssthresh " << m_ssThresh << " cwnd " << m_cWnd);
  // //     // fast retransmit in Scalable means triggering RTO earlier. Tx is restarted
  // //     // from the highest ack and run slow start again.
  // //     // (Fall & Floyd 1996, sec.1)
  // //     m_ssThresh = std::max (static_cast<unsigned> (m_cWnd / 2), m_segmentSize * 2);  // Half ssthresh
  // //     m_cWnd = m_segmentSize; // Run slow start again
  // //     m_nextTxSequence = m_txBuffer->HeadSequence (); // Restart from highest Ack
  // //     NS_LOG_INFO ("Triple Dup Ack: new ssthresh " << m_ssThresh << " cwnd " << m_cWnd);
  // //     NS_LOG_LOGIC ("Triple Dup Ack: retransmit missing segment at " << Simulator::Now ().GetSeconds ());
  // //     DoRetransmit ();
  // //   }

  NS_LOG_UNCOND ("DupAck " << count);
  if (count == 3)
    { // triple duplicate ack triggers fast retransmit (RFC2001, sec.3)
      NS_LOG_UNCOND ("Triple Dup Ack: old ssthresh " << m_ssThresh << " cwnd " << m_cWnd);
      // fast retransmit in Tahoe means triggering RTO earlier. Tx is restarted
      // from the highest ack and run slow start again.
      // (Fall & Floyd 1996, sec.1)
      m_ssThresh = std::max (static_cast<unsigned> (m_cWnd - m_cWnd / 8), m_segmentSize * 2);  // Half ssthresh
      m_cWnd = m_segmentSize; // Run slow start again
      m_nextTxSequence = m_txBuffer->HeadSequence (); // Restart from highest Ack
      NS_LOG_UNCOND ("Triple Dup Ack: new ssthresh " << m_ssThresh << " cwnd " << m_cWnd);
      NS_LOG_UNCOND ("Triple Dup Ack: retransmit missing segment at " << Simulator::Now ().GetSeconds ());
      DoRetransmit ();
    }
  NS_LOG_UNCOND ("CSV," << this << "," << Simulator::Now ().GetSeconds () << "," << m_cWnd << "," << m_ssThresh);
}

/* Retransmit timeout */
void TcpScalable::Retransmit (void)
{
  m_successfulAckCount = 0; // reset
  // // NS_LOG_FUNCTION (this);
  // // NS_LOG_LOGIC (this << " ReTxTimeout Expired at time " << Simulator::Now ().GetSeconds ());
  // // // m_inFastRec = false;

  // // If erroneous timeout in closed/timed-wait state, just return
  // if (m_state == CLOSED || m_state == TIME_WAIT) return;
  // // If all data are received (non-closing socket and nothing to send), just return
  // if (m_state <= ESTABLISHED && m_txBuffer->HeadSequence () >= m_highTxMark) return;

  // // According to RFC2581 sec.3.1, upon RTO, ssthresh is set to half of flight
  // // size and cwnd is set to 1*MSS, then the lost packet is retransmitted and
  // // TCP back to slow start
  // m_ssThresh = std::max (2 * m_segmentSize, BytesInFlight () - BytesInFlight () / 8);
  // m_cWnd = m_segmentSize;
  // // m_nextTxSequence = m_txBuffer->HeadSequence (); // Restart from highest Ack
  // NS_LOG_INFO ("RTO. Reset cwnd to " << m_cWnd <<
  //              ", ssthresh to " << m_ssThresh << ", restart from seqnum " << m_nextTxSequence);
  // DoRetransmit ();                          // Retransmit the packet

  NS_LOG_UNCOND ("ReTxTimeout Expired at time " << Simulator::Now ().GetSeconds ());
  // If erroneous timeout in closed/timed-wait state, just return
  if (m_state == CLOSED || m_state == TIME_WAIT) return;
  // If all data are received (non-closing socket and nothing to send), just return
  if (m_state <= ESTABLISHED && m_txBuffer->HeadSequence () >= m_highTxMark) return;

  m_ssThresh = std::max (static_cast<unsigned> (m_cWnd - m_cWnd / 8), m_segmentSize * 2);  // Half ssthresh
  m_cWnd = m_segmentSize;                   // Set cwnd to 1 segSize (RFC2001, sec.2)
  m_nextTxSequence = m_txBuffer->HeadSequence (); // Restart from highest Ack
  DoRetransmit ();                          // Retransmit the packet

  NS_LOG_UNCOND ("CSV," << this << "," << Simulator::Now ().GetSeconds () << "," << m_cWnd << "," << m_ssThresh);
}

} // namespace ns3
