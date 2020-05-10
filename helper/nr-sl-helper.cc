/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 *   Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2 as
 *   published by the Free Software Foundation;
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "nr-sl-helper.h"
#include <ns3/nr-sl-comm-resource-pool-factory.h>
#include <ns3/nr-sl-comm-preconfig-resource-pool-factory.h>
#include <ns3/nr-ue-net-device.h>
#include <ns3/lte-rrc-sap.h>
#include <ns3/nr-sl-ue-rrc.h>
#include <ns3/lte-ue-rrc.h>
#include <ns3/nr-ue-phy.h>
#include <ns3/nr-ue-mac.h>
#include <ns3/nr-amc.h>
#include <ns3/nr-spectrum-phy.h>
#include <ns3/lte-sl-tft.h>
#include <ns3/nr-point-to-point-epc-helper.h>
#include <ns3/nr-sl-bwp-manager-ue.h>

#include <ns3/fatal-error.h>
#include <ns3/log.h>
#include <ns3/abort.h>

#include <ns3/pointer.h>
#include <ns3/object-map.h>
#include <ns3/object-factory.h>
#include <ns3/simulator.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NrSlHelper");

NS_OBJECT_ENSURE_REGISTERED (NrSlHelper);

NrSlHelper::NrSlHelper (void)

{
  NS_LOG_FUNCTION (this);
  m_ueSlAmcFactory.SetTypeId (NrAmc::GetTypeId ());
}

NrSlHelper::~NrSlHelper (void)
{
  NS_LOG_FUNCTION (this);
}

TypeId
NrSlHelper::GetTypeId (void)
{
  static TypeId
    tid =
    TypeId ("ns3::NrSlHelper")
    .SetParent<Object> ()
    .SetGroupName ("nr")
    .AddConstructor<NrSlHelper> ();
  return tid;
}

void
NrSlHelper::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Object::DoDispose ();
}

void
NrSlHelper::SetSlErrorModel (const std::string &errorModelTypeId)
{
  NS_LOG_FUNCTION (this);

  SetUeSlAmcAttribute ("ErrorModelType", TypeIdValue (TypeId::LookupByName (errorModelTypeId)));
}

void
NrSlHelper::SetUeSlAmcAttribute (const std::string &n, const AttributeValue &v)
{
  NS_LOG_FUNCTION (this);
  m_ueSlAmcFactory.Set (n, v);
}

Ptr<NrAmc>
NrSlHelper::CreateUeSlAmc () const
{
  NS_LOG_FUNCTION (this);

  Ptr<NrAmc> slAmc = m_ueSlAmcFactory.Create <NrAmc> ();
  return slAmc;
}

void
NrSlHelper::SetEpcHelper (const Ptr<NrPointToPointEpcHelper> &epcHelper)
{
  NS_LOG_FUNCTION (this);
  m_epcHelper = epcHelper;

}

void
NrSlHelper::ActivateNrSlBearer (Time activationTime, NetDeviceContainer ues, const Ptr<LteSlTft> tft)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT_MSG (m_epcHelper, "NR Sidelink activation requires EpcHelper to be registered with the NrSlHelper");
  Simulator::Schedule (activationTime, &NrSlHelper::DoActivateNrSlBearer, this, ues, tft);
}

void
NrSlHelper::DoActivateNrSlBearer (NetDeviceContainer ues, const Ptr<LteSlTft> tft)
{
  NS_LOG_FUNCTION (this);
  for (NetDeviceContainer::Iterator i = ues.Begin (); i != ues.End (); ++i)
    {
      m_epcHelper->ActivateNrSlBearerForUe (*i, Create<LteSlTft> (tft)) ;
    }
}

void
NrSlHelper::PrepareUeForSidelink (NetDeviceContainer c, const std::set <uint8_t> &slBwpIds)
{
  NS_LOG_FUNCTION (this);
  for (NetDeviceContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<NetDevice> netDev = *i;
      Ptr<NrUeNetDevice> nrUeDev = netDev->GetObject <NrUeNetDevice>();
      PrepareSingleUeForSidelink (nrUeDev, slBwpIds);
    }

}

void
NrSlHelper::PrepareSingleUeForSidelink (Ptr<NrUeNetDevice> nrUeDev, const std::set <uint8_t> &slBwpIds)
{
  NS_LOG_FUNCTION (this);

  Ptr<LteUeRrc> lteUeRrc = nrUeDev->GetRrc ();

  Ptr<NrSlUeRrc> nrSlUeRrc = CreateObject<NrSlUeRrc> ();
  nrSlUeRrc->SetNrSlEnabled (true);
  nrSlUeRrc->SetNrSlUeRrcSapProvider (lteUeRrc->GetNrSlUeRrcSapProvider ());
  lteUeRrc->SetNrSlUeRrcSapUser (nrSlUeRrc->GetNrSlUeRrcSapUser ());
  uint64_t imsi = lteUeRrc->GetImsi ();
  NS_ASSERT_MSG (imsi != 0, "IMSI was not set in UE RRC");
  nrSlUeRrc->SetSourceL2Id (static_cast <uint32_t> (imsi & 0xFFFFFF)); //use lower 24 bits of IMSI as source

  //Aggregate
  lteUeRrc->AggregateObject (nrSlUeRrc);
  //SL BWP manager configuration
  Ptr<NrSlBwpManagerUe> slBwpManager = DynamicCast<NrSlBwpManagerUe> (nrUeDev->GetBwpManager ());
  slBwpManager->SetNrSlUeBwpmRrcSapUser (lteUeRrc->GetNrSlUeBwpmRrcSapUser ());
  lteUeRrc->SetNrSlUeBwpmRrcSapProvider (slBwpManager->GetNrSlUeBwpmRrcSapProvider ());

  lteUeRrc->SetNrSlMacSapProvider (slBwpManager->GetNrSlMacSapProviderFromBwpm ());

  //Error model and UE MAC AMC
  Ptr<NrAmc> slAmc = CreateUeSlAmc ();
  TypeIdValue typeIdValue;
  slAmc->GetAttribute ("ErrorModelType", typeIdValue);

  for (const auto &itBwps:slBwpIds)
    {
      //Store BWP id in NrSlUeRrc
      nrUeDev->GetRrc()->GetObject <NrSlUeRrc> ()->StoreSlBwpId (itBwps);

      lteUeRrc->SetNrSlUeCmacSapProvider (itBwps, nrUeDev->GetMac (itBwps)->GetNrSlUeCmacSapProvider ());
      nrUeDev->GetMac (itBwps)->SetNrSlUeCmacSapUser (lteUeRrc->GetNrSlUeCmacSapUser ());

      nrUeDev->GetPhy (itBwps)->SetNrSlUeCphySapUser (lteUeRrc->GetNrSlUeCphySapUser ());
      lteUeRrc->SetNrSlUeCphySapProvider (itBwps, nrUeDev->GetPhy (itBwps)->GetNrSlUeCphySapProvider ());

      nrUeDev->GetPhy (itBwps)->SetNrSlUePhySapUser (nrUeDev->GetMac (itBwps)->GetNrSlUePhySapUser ());
      nrUeDev->GetMac (itBwps)->SetNrSlUePhySapProvider (nrUeDev->GetPhy (itBwps)->GetNrSlUePhySapProvider ());

      nrUeDev->GetPhy (itBwps)->GetSpectrumPhy ()->SetAttribute ("SlErrorModelType", typeIdValue);
      nrUeDev->GetMac (itBwps)->SetSlAmcModel (slAmc);

      bool bwpmTest = slBwpManager->SetNrSlMacSapProviders (itBwps, nrUeDev->GetMac (itBwps)->GetNrSlMacSapProvider ());

      if (bwpmTest == false)
        {
          NS_FATAL_ERROR ("Error in SetNrSlMacSapProviders");
        }
    }

  lteUeRrc->SetNrSlBwpIdContainerInBwpm ();
}

void
NrSlHelper::InstallNrSlPreConfiguration (NetDeviceContainer c, const LteRrcSap::SidelinkPreconfigNr preConfig)
{
  NS_LOG_FUNCTION (this);

  struct LteRrcSap::SlFreqConfigCommonNr slFreqConfigCommonNr = preConfig.slPreconfigFreqInfoList [0];
  LteRrcSap::SlPreconfigGeneralNr slPreconfigGeneralNr = preConfig.slPreconfigGeneral;

  for (NetDeviceContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<NetDevice> netDev = *i;
      Ptr<NrUeNetDevice> nrUeDev = netDev->GetObject <NrUeNetDevice>();
      Ptr<LteUeRrc> lteUeRrc = nrUeDev->GetRrc ();
      Ptr<NrSlUeRrc> nrSlUeRrc = lteUeRrc->GetObject <NrSlUeRrc> ();
      nrSlUeRrc->SetNrSlPreconfiguration (preConfig);
      bool ueSlBwpConfigured = ConfigUeParams (nrUeDev, slFreqConfigCommonNr, slPreconfigGeneralNr);
      NS_ABORT_MSG_IF (ueSlBwpConfigured == false, "No SL configuration found for IMSI " << nrUeDev->GetImsi ());
    }
}

bool
NrSlHelper::ConfigUeParams (const Ptr<NrUeNetDevice> &dev,
                            const LteRrcSap::SlFreqConfigCommonNr &freqCommon,
                            const LteRrcSap::SlPreconfigGeneralNr &general)
{
  NS_LOG_FUNCTION (this);
  bool found = false;
  std::string tddPattern = general.slTddConfig.tddPattern;
  //Sanity check: Here we are retrieving the BWP id container
  //from UE RRC to make sure:
  //1. PrepareUeForSidelink has been called already
  //2. In the for loop below the index (slBwpList [index]) at which we find the
  //configuration is basically the index of the BWP, which user want use for SL.
  //So, this index should be present in the BWP id container.
  Ptr<LteUeRrc> lteUeRrc = dev->GetRrc ();
  std::set <uint8_t> bwpIds = lteUeRrc->GetNrSlBwpIdContainer ();

  for (uint8_t index = 0; index < freqCommon.slBwpList.size (); ++index)
    {
      //configure the parameters if both BWP generic and SL pools are configured.
      if (freqCommon.slBwpList [index].haveSlBwpGeneric && freqCommon.slBwpList [index].haveSlBwpPoolConfigCommonNr)
        {
          NS_LOG_INFO ("Configuring BWP id " << +index << " for SL");
          auto it = bwpIds.find (index);
          NS_ABORT_MSG_IF (it == bwpIds.end (), "UE is not prepared to use BWP id " << +index << " for SL");
          dev->GetPhy (index)->RegisterSlBwpId (static_cast <uint16_t> (index));
          dev->GetPhy (index)->SetNumerology (freqCommon.slBwpList [index].slBwpGeneric.bwp.numerology);
          dev->GetPhy (index)->SetSymbolsPerSlot (freqCommon.slBwpList [index].slBwpGeneric.bwp.symbolsPerSlots);
          dev->GetPhy (index)->PreConfigSlBandwidth (freqCommon.slBwpList [index].slBwpGeneric.bwp.bandwidth);
          dev->GetPhy (index)->SetNumRbPerRbg (freqCommon.slBwpList [index].slBwpGeneric.bwp.rbPerRbg);
          dev->GetPhy (index)->SetPattern (tddPattern);
          found = true;
        }
    }

  return found;
}


} // namespace ns3
