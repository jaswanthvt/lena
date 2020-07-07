/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 *   Copyright (c) 2020 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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

/**
 * \ingroup examples
 * \file s3-scenario.cc
 * \brief A multi-cell network deployment with site sectorization
 *
 * This example describes how to setup a simulation using the 3GPP channel model
 * from TR 38.900. This example consists of an hexagonal grid deployment
 * consisting on a central site and a number of outer rings of sites around this
 * central site. Each site is sectorized, meaning that a number of three antenna
 * arrays or panels are deployed per gNB. These three antennas are pointing to
 * 30º, 150º and 270º w.r.t. the horizontal axis. We allocate a band to each
 * sector of a site, and the bands are contiguous in frequency.
 *
 * We provide a number of simulation parameters that can be configured in the
 * command line, such as the number of UEs per cell or the number of outer rings.
 * Please have a look at the possible parameters to know what you can configure
 * through the command line.
 *
 * With the default configuration, the example will create one DL flow per UE.
 * The example will print on-screen the end-to-end result of each flow,
 * as well as writing them on a file.
 *
 * \code{.unparsed}
$ ./waf --run "lena-lte-comparison --Help"
    \endcode
 *
 */

/*
 * Include part. Often, you will have to include the headers for an entire module;
 * do that by including the name of the module you need with the suffix "-module.h".
 */

#include "ns3/core-module.h"
#include "ns3/config-store.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/nr-module.h"
#include "ns3/lte-module.h"
#include <ns3/radio-environment-map-helper.h>
#include "ns3/config-store-module.h"
#include <ns3/sqlite-output.h>
#include "radio-network-parameters-helper.h"
#include "sinr-output-stats.h"
#include "flow-monitor-output-stats.h"
#include "power-output-stats.h"
#include "slot-output-stats.h"

/*
 * To be able to use LOG_* functions.
 */
#include "ns3/log.h"

/*
 * Use, always, the namespace ns3. All the NR classes are inside such namespace.
 */
using namespace ns3;

/*
 * With this line, we will be able to see the logs of the file by enabling the
 * component "CttcNrDemo", in this way:
 *
 * $ export NS_LOG="LenaLteComparison=level_info|prefix_func|prefix_time"
 */
NS_LOG_COMPONENT_DEFINE ("LenaLteComparison");

static void
ReportSinrNr (SinrOutputStats *stats, uint16_t cellId, uint16_t rnti,
              double power, double avgSinr, uint16_t bwpId)
{
  stats->SaveSinr (cellId, rnti, power, avgSinr, bwpId);
}

static void
ReportSinrLena (SinrOutputStats *stats, uint16_t cellId, uint16_t rnti, double power, double avgSinr, uint8_t bwpId)
{
  ReportSinrNr (stats, cellId, rnti, power, avgSinr, static_cast<uint16_t> (bwpId));
}

static void
ReportPowerNr (PowerOutputStats *stats, const SfnSf & sfnSf,
               Ptr<SpectrumValue> txPsd, Time t, uint16_t rnti, uint64_t imsi,
               uint16_t bwpId, uint16_t cellId)
{
  stats->SavePower (sfnSf, txPsd, t, rnti, imsi, bwpId, cellId);
}

static void
ReportPowerLena (PowerOutputStats *stats, uint16_t rnti, Ptr<SpectrumValue> txPsd)
{
  // Please note that LENA has less output than NR... so we have to save less information.
  ReportPowerNr (stats, SfnSf (), txPsd, MilliSeconds (0), rnti, 0, 0, 0);
}

static void
ReportSlotStatsNr (SlotOutputStats *stats, const SfnSf &sfnSf, uint32_t scheduledUe,
                   uint32_t usedReg, uint32_t usedSym,
                   uint32_t availableRb, uint32_t availableSym, uint16_t bwpId,
                   uint16_t cellId)
{
  stats->SaveSlotStats (sfnSf, scheduledUe, usedReg, usedSym,
                        availableRb, availableSym, bwpId, cellId);
}

void SetLenaSimulatorParameters (HexagonalGridScenarioHelper gridScenario,
                                 std::string scenario,
                                 NodeContainer enbSector1Container,
                                 NodeContainer enbSector2Container,
                                 NodeContainer enbSector3Container,
                                 NodeContainer ueSector1Container,
                                 NodeContainer ueSector2Container,
                                 NodeContainer ueSector3Container,
                                 Ptr<PointToPointEpcHelper> &epcHelper,
                                 Ptr<LteHelper> &lteHelper,
                                 NetDeviceContainer &enbSector1NetDev,
                                 NetDeviceContainer &enbSector2NetDev,
                                 NetDeviceContainer &enbSector3NetDev,
                                 NetDeviceContainer &ueSector1NetDev,
                                 NetDeviceContainer &ueSector2NetDev,
                                 NetDeviceContainer &ueSector3NetDev,
                                 bool calibration,
                                 SinrOutputStats *sinrStats,
                                 PowerOutputStats *powerStats,
                                 const std::string &scheduler,
                                 uint32_t bandwidthMHz)
{

  /*
   *  An example of how the spectrum is being used.
   *
   *                              centralEarfcnFrequencyBand = 350
   *                                     |
   *         200 RB                    200 RB                 200RB
   * |-----------------------|-----------------------|-----------------------|
   *
   *     100RB      100RB        100RB       100RB       100RB       100RB
   * |-----------|-----------|-----------|-----------|-----------|-----------|
   *       DL          UL          DL         UL           DL         UL
   *
   * |-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|
   *     fc_dl       fc_ul       fc_dl       fc_ul        fc_dl      fc_ul
   */

  uint32_t bandwidthBandDlRB;
  uint32_t bandwidthBandUlRB;

  if (bandwidthMHz == 20)
    {
      bandwidthBandDlRB = 100;
      bandwidthBandUlRB = 100;
    }
  else if (bandwidthMHz == 15)
    {
      bandwidthBandDlRB = 75;
      bandwidthBandUlRB = 75;
    }
  else if (bandwidthMHz == 10)
    {
      bandwidthBandDlRB = 50;
      bandwidthBandUlRB = 50;
    }
  else if (bandwidthMHz == 5)
    {
      bandwidthBandDlRB = 25;
      bandwidthBandUlRB = 25;
    }
  else
    {
      NS_ABORT_MSG ("The configured bandwidth in MHz not supported:" << bandwidthMHz);
    }

  uint32_t centralFrequencyBand1Dl = 100;
  uint32_t centralFrequencyBand1Ul = 200;
  uint32_t centralFrequencyBand2Dl = 300;
  uint32_t centralFrequencyBand2Ul = 400;
  uint32_t centralFrequencyBand3Dl = 500;
  uint32_t centralFrequencyBand3Ul = 600;

  double txPower;
  double ueTxPower = 23;
  std::string pathlossModel;
  if (scenario == "UMa")
    {
      txPower = 43;
      pathlossModel = "ns3::ThreeGppUmaPropagationLossModel";
    }
  else if (scenario == "UMi")
    {
      txPower = 44;
      pathlossModel = "ns3::ThreeGppUmiStreetCanyonPropagationLossModel";
    }
  else if (scenario == "RMa")
    {
      txPower = 43;
      pathlossModel = "ns3::ThreeGppRmaPropagationLossModel";
    }
  else
    {
      NS_FATAL_ERROR("Selected scenario " << scenario << " not valid. Valid values: UMa, UMi, RMa");
    }

  lteHelper = CreateObject<LteHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  // ALL SECTORS AND BANDS configuration
  Config::SetDefault ("ns3::FfMacScheduler::UlCqiFilter", EnumValue (FfMacScheduler::PUSCH_UL_CQI));
  Config::SetDefault ("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue (false));
  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(999999999));
  Config::SetDefault ("ns3::LteEnbPhy::TxPower", DoubleValue (txPower));
  Config::SetDefault ("ns3::LteUePhy::TxPower", DoubleValue (ueTxPower));
  Config::SetDefault ("ns3::LteUePhy::NoiseFigure", DoubleValue (9.0));
  Config::SetDefault ("ns3::LteUePhy::EnableRlfDetection", BooleanValue (false));
  Config::SetDefault ("ns3::LteAmc::AmcModel", EnumValue(LteAmc::PiroEW2010));
  lteHelper->SetAttribute ("PathlossModel", StringValue (pathlossModel)); // for each band the same pathloss model

  // Disable shadowing in calibration, and enable it in non-calibration mode
  lteHelper->SetPathlossModelAttribute ("ShadowingEnabled", BooleanValue (!calibration));

  if (scheduler == "PF")
    {
      lteHelper->SetSchedulerType ("ns3::PfFfMacScheduler");
    }
  else if (scheduler == "RR")
    {
      lteHelper->SetSchedulerType ("ns3::RrFfMacScheduler");
    }

  if (calibration)
    {
      lteHelper->SetEnbAntennaModelType ("ns3::IsotropicAntennaModel");
      Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));
    }
  else
    {
      lteHelper->SetEnbAntennaModelType ("ns3::CosineAntennaModel");
      lteHelper->SetEnbAntennaModelAttribute ("Beamwidth", DoubleValue (130));
      lteHelper->SetEnbAntennaModelAttribute ("MaxGain", DoubleValue (0));
    }
  lteHelper->SetEnbDeviceAttribute ("DlBandwidth", UintegerValue (bandwidthBandDlRB));
  lteHelper->SetEnbDeviceAttribute ("UlBandwidth", UintegerValue (bandwidthBandUlRB));

  //SECTOR 1 eNB configuration
  if (!calibration)
    {
      double orientationDegrees = gridScenario.GetAntennaOrientationDegrees (0, gridScenario.GetNumSectorsPerSite ());
      lteHelper->SetEnbAntennaModelAttribute ("Orientation", DoubleValue (orientationDegrees));
    }
  lteHelper->SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (centralFrequencyBand1Dl));
  lteHelper->SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (centralFrequencyBand1Ul));
  enbSector1NetDev = lteHelper->InstallEnbDevice (enbSector1Container);

  //SECTOR 2 eNB configuration
  if (!calibration)
    {
      double orientationDegrees = gridScenario.GetAntennaOrientationDegrees (1, gridScenario.GetNumSectorsPerSite ());
      lteHelper->SetEnbAntennaModelAttribute ("Orientation", DoubleValue (orientationDegrees));
    }

  lteHelper->SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (centralFrequencyBand2Dl));
  lteHelper->SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (centralFrequencyBand2Ul));
  enbSector2NetDev = lteHelper->InstallEnbDevice (enbSector2Container);

  //SECTOR 3 eNB configuration
  if (!calibration)
    {
      double orientationDegrees = gridScenario.GetAntennaOrientationDegrees (2, gridScenario.GetNumSectorsPerSite ());
      lteHelper->SetEnbAntennaModelAttribute ("Orientation", DoubleValue (orientationDegrees));
    }
  lteHelper->SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (centralFrequencyBand3Dl));
  lteHelper->SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (centralFrequencyBand3Ul));
  enbSector3NetDev = lteHelper->InstallEnbDevice (enbSector3Container);

  ueSector1NetDev = lteHelper->InstallUeDevice(ueSector1Container);
  ueSector2NetDev = lteHelper->InstallUeDevice(ueSector2Container);
  ueSector3NetDev = lteHelper->InstallUeDevice(ueSector3Container);

  for (uint32_t i = 0; i < ueSector1NetDev.GetN (); i++)
    {
      auto ueNetDevice = DynamicCast<LteUeNetDevice> (ueSector1NetDev.Get (i));
      NS_ASSERT (ueNetDevice->GetCcMap ().size () == 1);
      auto uePhy = ueNetDevice->GetPhy ();

      uePhy->TraceConnectWithoutContext ("ReportCurrentCellRsrpSinr", MakeBoundCallback (&ReportSinrLena, sinrStats));
      uePhy->TraceConnectWithoutContext ("ReportPowerSpectralDensity", MakeBoundCallback (&ReportPowerLena, powerStats));
    }

  for (uint32_t i = 0; i < ueSector2NetDev.GetN (); i++)
    {
      auto ueNetDevice = DynamicCast<LteUeNetDevice> (ueSector2NetDev.Get (i));
      NS_ASSERT (ueNetDevice->GetCcMap ().size () == 1);
      auto uePhy = ueNetDevice->GetPhy ();

      uePhy->TraceConnectWithoutContext ("ReportCurrentCellRsrpSinr", MakeBoundCallback (&ReportSinrLena, sinrStats));
      uePhy->TraceConnectWithoutContext ("ReportPowerSpectralDensity", MakeBoundCallback (&ReportPowerLena, powerStats));
    }

  for (uint32_t i = 0; i < ueSector3NetDev.GetN (); i++)
    {
      auto ueNetDevice = DynamicCast<LteUeNetDevice> (ueSector3NetDev.Get (i));
      NS_ASSERT (ueNetDevice->GetCcMap ().size () == 1);
      auto uePhy = ueNetDevice->GetPhy ();

      uePhy->TraceConnectWithoutContext ("ReportCurrentCellRsrpSinr", MakeBoundCallback (&ReportSinrLena, sinrStats));
      uePhy->TraceConnectWithoutContext ("ReportPowerSpectralDensity", MakeBoundCallback (&ReportPowerLena, powerStats));
    }

  lteHelper->Initialize ();
  auto dlSp = DynamicCast<ThreeGppPropagationLossModel> (lteHelper->GetDownlinkSpectrumChannel ()->GetPropagationLossModel());
  auto ulSp = DynamicCast<ThreeGppPropagationLossModel> (lteHelper->GetUplinkSpectrumChannel ()->GetPropagationLossModel());

  NS_ASSERT (dlSp != nullptr);
  NS_ASSERT (ulSp != nullptr);

  NS_ASSERT (dlSp->GetNext() == nullptr);
  NS_ASSERT (ulSp->GetNext() == nullptr);

  ObjectFactory f;
  f.SetTypeId (TypeId::LookupByName("ns3::AlwaysLosChannelConditionModel"));
  dlSp->SetChannelConditionModel (f.Create<ChannelConditionModel> ());
  ulSp->SetChannelConditionModel (f.Create<ChannelConditionModel> ());
}


void Set5gLenaSimulatorParameters (const HexagonalGridScenarioHelper &gridScenario,
                                   const std::string &scenario,
                                   const std::string &radioNetwork,
                                   std::string &errorModel,
                                   const std::string &operationMode,
                                   const std::string &direction,
                                   uint16_t numerology,
                                   const std::string &pattern,
                                   const NodeContainer &gnbSector1Container,
                                   const NodeContainer &gnbSector2Container,
                                   const NodeContainer &gnbSector3Container,
                                   const NodeContainer &ueSector1Container,
                                   const NodeContainer &ueSector2Container,
                                   const NodeContainer &ueSector3Container,
                                   const Ptr<PointToPointEpcHelper> &baseEpcHelper,
                                   Ptr<NrHelper> &nrHelper,
                                   NetDeviceContainer &gnbSector1NetDev,
                                   NetDeviceContainer &gnbSector2NetDev,
                                   NetDeviceContainer &gnbSector3NetDev,
                                   NetDeviceContainer &ueSector1NetDev,
                                   NetDeviceContainer &ueSector2NetDev,
                                   NetDeviceContainer &ueSector3NetDev,
                                   bool calibration,
                                   SinrOutputStats *sinrStats,
                                   PowerOutputStats *powerStats,
                                   SlotOutputStats *slotStats,
                                   const std::string &scheduler,
                                   uint32_t bandwidthMHz)
{



  /*
   * Create the radio network related parameters
   */
  RadioNetworkParametersHelper ranHelper;
  uint8_t numScPerRb = 1;  //!< The reference signal density is different in LTE and in NR
  double rbOverhead = 0.1;
  uint32_t harqProcesses = 20;
  uint32_t n1Delay = 2;
  uint32_t n2Delay = 2;
  ranHelper.SetScenario (scenario);
  if (radioNetwork == "LTE")
    {
      ranHelper.SetNetworkToLte (operationMode, 1, bandwidthMHz);
      rbOverhead = 0.1;
      harqProcesses = 8;
      n1Delay = 4;
      n2Delay = 4;
      if (errorModel == "")
        {
          errorModel = "ns3::LenaErrorModel";
        }
      else if (errorModel != "ns3::NrLteMiErrorModel" && errorModel != "ns3::LenaErrorModel")
        {
          NS_ABORT_MSG ("The selected error model is not recommended for LTE");
        }
    }
  else if (radioNetwork == "NR")
    {
      ranHelper.SetNetworkToNr (operationMode, numerology, 1, bandwidthMHz);
      rbOverhead = 0.04;
      harqProcesses = 20;
      if (errorModel == "")
        {
          errorModel = "ns3::NrEesmCcT2";
        }
      else if (errorModel == "ns3::NrLteMiErrorModel")
        {
          NS_ABORT_MSG ("The selected error model is not recommended for NR");
        }
    }
  else
    {
      NS_ABORT_MSG ("Unrecognized radio network technology");
    }

  /*
   * Setup the NR module. We create the various helpers needed for the
   * NR simulation:
   * - IdealBeamformingHelper, which takes care of the beamforming part
   * - NrHelper, which takes care of creating and connecting the various
   * part of the NR stack
   */

  Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
  nrHelper = CreateObject<NrHelper> ();

  // Put the pointers inside nrHelper
  nrHelper->SetIdealBeamformingHelper (idealBeamformingHelper);

  Ptr<NrPointToPointEpcHelper> epcHelper = DynamicCast<NrPointToPointEpcHelper> (baseEpcHelper);
  nrHelper->SetEpcHelper (epcHelper);

  /*
   * Spectrum division. We create one operational band containing three
   * component carriers, and each CC containing a single bandwidth part
   * centered at the frequency specified by the input parameters.
   * Each spectrum part length is, as well, specified by the input parameters.
   * The operational band will use StreetCanyon channel or UrbanMacro modeling.
   */
  BandwidthPartInfoPtrVector allBwps, bwps1, bwps2, bwps3;
  CcBwpCreator ccBwpCreator;
  // Create the configuration for the CcBwpHelper. SimpleOperationBandConf creates
  // a single BWP per CC. Get the spectrum values from the RadioNetworkParametersHelper
  double centralFrequencyBand = ranHelper.GetCentralFrequency ();
  double bandwidthBand = ranHelper.GetBandwidth ();
  const uint8_t numCcPerBand = 1; // In this example, each cell will have one CC with one BWP
  BandwidthPartInfo::Scenario scene;
  if (scenario == "UMi")
    {
      scene =  BandwidthPartInfo::UMi_StreetCanyon_LoS;
    }
  else if (scenario == "UMa")
    {
      scene = BandwidthPartInfo::UMa_LoS;
    }
  else if (scenario == "RMa")
    {
      scene = BandwidthPartInfo::RMa_LoS;
    }
  else
    {
      NS_ABORT_MSG ("Unsupported scenario " << scenario << ". Supported values: UMi, UMa, RMa");
    }

  /*
   * Attributes of ThreeGppChannelModel still cannot be set in our way.
   * TODO: Coordinate with Tommaso
   */
  Config::SetDefault ("ns3::ThreeGppChannelModel::UpdatePeriod",TimeValue (MilliSeconds(100)));
  nrHelper->SetChannelConditionModelAttribute ("UpdatePeriod", TimeValue (MilliSeconds (0)));

  // Disable shadowing in calibration, and enable it in non-calibration mode
  nrHelper->SetPathlossAttribute ("ShadowingEnabled", BooleanValue (!calibration));

  // Noise figure for the UE
  nrHelper->SetUePhyAttribute ("NoiseFigure", DoubleValue (9.0));

  // Error Model: UE and GNB with same spectrum error model.
  nrHelper->SetUlErrorModel (errorModel);
  nrHelper->SetDlErrorModel (errorModel);

  // Both DL and UL AMC will have the same model behind.
  nrHelper->SetGnbDlAmcAttribute ("AmcModel", EnumValue (NrAmc::ShannonModel));
  nrHelper->SetGnbUlAmcAttribute ("AmcModel", EnumValue (NrAmc::ShannonModel));

  /*
   * Adjust the average number of Reference symbols per RB only for LTE case,
   * which is larger than in NR. We assume a value of 4 (could be 3 too).
   */
  nrHelper->SetGnbDlAmcAttribute ("NumRefScPerRb", UintegerValue (numScPerRb));
  nrHelper->SetGnbUlAmcAttribute ("NumRefScPerRb", UintegerValue (1));  //FIXME: Might change in LTE

  nrHelper->SetGnbPhyAttribute ("RbOverhead", DoubleValue (rbOverhead));
  nrHelper->SetGnbPhyAttribute ("N2Delay", UintegerValue (n2Delay));
  nrHelper->SetGnbPhyAttribute ("N1Delay", UintegerValue (n1Delay));

  nrHelper->SetUeMacAttribute ("NumHarqProcess", UintegerValue (harqProcesses));
  nrHelper->SetGnbMacAttribute ("NumHarqProcess", UintegerValue (harqProcesses));

  /*
   * Create the necessary operation bands. In this example, each sector operates
   * in a separate band. Each band contains a single component carrier (CC),
   * which is made of one BWP in TDD operation mode or two BWPs in FDD mode.
   * Note that BWPs have the same bandwidth. Therefore, CCs and bands in FDD are
   * twice larger than in TDD.
   *
   * The configured spectrum division for TDD operation is:
   * |---Band1---|---Band2---|---Band3---|
   * |----CC1----|----CC2----|----CC3----|
   * |----BWP1---|----BWP2---|----BWP3---|
   *
   * And the configured spectrum division for FDD operation is:
   * |---------Band1---------|---------Band2---------|---------Band3---------|
   * |----------CC1----------|----------CC2----------|----------CC3----------|
   * |----BWP1---|----BWP2---|----BWP3---|----BWP4---|----BWP5---|----BWP6---|
   */
  double centralFrequencyBand1 = centralFrequencyBand - bandwidthBand;
  double centralFrequencyBand2 = centralFrequencyBand;
  double centralFrequencyBand3 = centralFrequencyBand + bandwidthBand;
  double bandwidthBand1 = bandwidthBand;
  double bandwidthBand2 = bandwidthBand;
  double bandwidthBand3 = bandwidthBand;

  uint8_t numBwpPerCc = 1;
  if (operationMode == "FDD")
    {
      numBwpPerCc = 2; // FDD will have 2 BWPs per CC
    }

  CcBwpCreator::SimpleOperationBandConf bandConf1 (centralFrequencyBand1, bandwidthBand1, numCcPerBand, scene);
  bandConf1.m_numBwp = numBwpPerCc; // FDD will have 2 BWPs per CC
  CcBwpCreator::SimpleOperationBandConf bandConf2 (centralFrequencyBand2, bandwidthBand2, numCcPerBand, scene);
  bandConf2.m_numBwp = numBwpPerCc; // FDD will have 2 BWPs per CC
  CcBwpCreator::SimpleOperationBandConf bandConf3 (centralFrequencyBand3, bandwidthBand3, numCcPerBand, scene);
  bandConf3.m_numBwp = numBwpPerCc; // FDD will have 2 BWPs per CC

  // By using the configuration created, it is time to make the operation bands
  OperationBandInfo band1 = ccBwpCreator.CreateOperationBandContiguousCc (bandConf1);
  OperationBandInfo band2 = ccBwpCreator.CreateOperationBandContiguousCc (bandConf2);
  OperationBandInfo band3 = ccBwpCreator.CreateOperationBandContiguousCc (bandConf3);

  if (calibration)
    {
      band1.m_cc[0]->m_bwp[0]->m_centralFrequency = 2.16e+09;
      band1.m_cc[0]->m_bwp[1]->m_centralFrequency = 1.93e+09;
      band2.m_cc[0]->m_bwp[0]->m_centralFrequency = 2.16e+09;
      band2.m_cc[0]->m_bwp[1]->m_centralFrequency = 1.93e+09;
      band3.m_cc[0]->m_bwp[0]->m_centralFrequency = 2.16e+09;
      band3.m_cc[0]->m_bwp[1]->m_centralFrequency = 1.93e+09;

      // Do not initialize fading (beamforming gain)
      nrHelper->InitializeOperationBand (&band1, NrHelper::INIT_PROPAGATION | NrHelper::INIT_CHANNEL);
      nrHelper->InitializeOperationBand (&band2, NrHelper::INIT_PROPAGATION | NrHelper::INIT_CHANNEL);
      nrHelper->InitializeOperationBand (&band3, NrHelper::INIT_PROPAGATION | NrHelper::INIT_CHANNEL);
    }
  else
    {
      // Init everything
      nrHelper->InitializeOperationBand (&band1);
      nrHelper->InitializeOperationBand (&band2);
      nrHelper->InitializeOperationBand (&band3);
    }


  allBwps = CcBwpCreator::GetAllBwps ({band1,band2,band3});
  bwps1 = CcBwpCreator::GetAllBwps ({band1});
  bwps2 = CcBwpCreator::GetAllBwps ({band2});
  bwps3 = CcBwpCreator::GetAllBwps ({band3});

  /*
   * Start to account for the bandwidth used by the example, as well as
   * the total power that has to be divided among the BWPs. Since there is only
   * one band and one BWP occupying the entire band, there is no need to divide
   * power among BWPs.
   */
  double totalTxPower = ranHelper.GetTxPower (); //Convert to mW
  double x = pow (10, totalTxPower/10);

  /*
   * allBwps contains all the spectrum configuration needed for the nrHelper.
   *
   * Now, we can setup the attributes. We can have three kind of attributes:
   * (i) parameters that are valid for all the bandwidth parts and applies to
   * all nodes, (ii) parameters that are valid for all the bandwidth parts
   * and applies to some node only, and (iii) parameters that are different for
   * every bandwidth parts. The approach is:
   *
   * - for (i): Configure the attribute through the helper, and then install;
   * - for (ii): Configure the attribute through the helper, and then install
   * for the first set of nodes. Then, change the attribute through the helper,
   * and install again;
   * - for (iii): Install, and then configure the attributes by retrieving
   * the pointer needed, and calling "SetAttribute" on top of such pointer.
   *
   */

  /*
   *  Case (i): Attributes valid for all the nodes
   */
  // Beamforming method
  if (radioNetwork == "LTE")
    {
      idealBeamformingHelper->SetAttribute ("IdealBeamformingMethod", TypeIdValue (QuasiOmniDirectPathBeamforming::GetTypeId ()));
    }
  else
    {
      idealBeamformingHelper->SetAttribute ("IdealBeamformingMethod", TypeIdValue (DirectPathBeamforming::GetTypeId ()));
    }

  // Scheduler type

  if (scheduler == "PF")
    {
      nrHelper->SetSchedulerTypeId (TypeId::LookupByName ("ns3::NrMacSchedulerOfdmaPF"));
    }
  else if (scheduler == "RR")
    {
      nrHelper->SetSchedulerTypeId (TypeId::LookupByName ("ns3::NrMacSchedulerOfdmaRR"));
    }

  nrHelper->SetSchedulerAttribute ("DlCtrlSymbols", UintegerValue (1));

  // Core latency
  epcHelper->SetAttribute ("S1uLinkDelay", TimeValue (MilliSeconds (0)));

  // Antennas for all the UEs
  nrHelper->SetUeAntennaAttribute ("NumRows", UintegerValue (1));
  nrHelper->SetUeAntennaAttribute ("NumColumns", UintegerValue (1));
  nrHelper->SetUeAntennaAttribute ("IsotropicElements", BooleanValue (true));
  nrHelper->SetUeAntennaAttribute ("ElementGain", DoubleValue (0));

  // Antennas for all the gNbs
  nrHelper->SetGnbAntennaAttribute ("NumRows", UintegerValue (1));
  nrHelper->SetGnbAntennaAttribute ("NumColumns", UintegerValue (1));
  nrHelper->SetGnbAntennaAttribute ("IsotropicElements", BooleanValue (false));
  nrHelper->SetGnbAntennaAttribute ("ElementGain", DoubleValue (0));

  // UE transmit power
  nrHelper->SetUePhyAttribute ("TxPower", DoubleValue (23.0));

  // Set LTE RBG size
  if (radioNetwork == "LTE")
    {
      double singleCcBw = numBwpPerCc == 2 ? bandwidthBand / 2 : bandwidthBand;

      if (singleCcBw == 20e6)
        {
          nrHelper->SetGnbMacAttribute ("NumRbPerRbg", UintegerValue (4));
        }
      else if (singleCcBw == 15e6)
        {
          nrHelper->SetGnbMacAttribute ("NumRbPerRbg", UintegerValue (4));
        }
      else if (singleCcBw == 10e6)
        {
          nrHelper->SetGnbMacAttribute ("NumRbPerRbg", UintegerValue (3));
        }
      else if (singleCcBw == 5e6)
        {
          nrHelper->SetGnbMacAttribute ("NumRbPerRbg", UintegerValue (2));
        }
      else
        {
          NS_ABORT_MSG ("Currently, only supported bandwidths are 5, 10, 15, and 20MHz, you chose " << singleCcBw);
        }
    }

  // We assume a common traffic pattern for all UEs
  uint32_t bwpIdForLowLat = 0;
  if (operationMode == "FDD" && direction == "UL")
    {
      bwpIdForLowLat = 1;
    }

  // gNb routing between Bearer and bandwidth part
  nrHelper->SetGnbBwpManagerAlgorithmAttribute ("NGBR_VIDEO_TCP_DEFAULT", UintegerValue (bwpIdForLowLat));

  // Ue routing between Bearer and bandwidth part
  nrHelper->SetUeBwpManagerAlgorithmAttribute ("NGBR_VIDEO_TCP_DEFAULT", UintegerValue (bwpIdForLowLat));

  /*
   * We miss many other parameters. By default, not configuring them is equivalent
   * to use the default values. Please, have a look at the documentation to see
   * what are the default values for all the attributes you are not seeing here.
   */

  /*
   * Case (ii): Attributes valid for a subset of the nodes
   */

  // NOT PRESENT IN THIS SIMPLE EXAMPLE

  /*
   * We have configured the attributes we needed. Now, install and get the pointers
   * to the NetDevices, which contains all the NR stack:
   */

  //  NetDeviceContainer enbNetDev = nrHelper->InstallGnbDevice (gridScenario.GetBaseStations (), allBwps);
  gnbSector1NetDev = nrHelper->InstallGnbDevice (gnbSector1Container, bwps1);
  gnbSector2NetDev = nrHelper->InstallGnbDevice (gnbSector2Container, bwps2);
  gnbSector3NetDev = nrHelper->InstallGnbDevice (gnbSector3Container, bwps3);
  ueSector1NetDev = nrHelper->InstallUeDevice (ueSector1Container, bwps1);
  ueSector2NetDev = nrHelper->InstallUeDevice (ueSector2Container, bwps2);
  ueSector3NetDev = nrHelper->InstallUeDevice (ueSector3Container, bwps3);

  /*
   * Case (iii): Go node for node and change the attributes we have to setup
   * per-node.
   */

  // Sectors (cells) of a site are pointing at different directions
  double orientationRads = gridScenario.GetAntennaOrientationRadians (0, gridScenario.GetNumSectorsPerSite ());
  for (uint32_t numCell = 0; numCell < gnbSector1NetDev.GetN (); ++numCell)
    {
      Ptr<NetDevice> gnb = gnbSector1NetDev.Get (numCell);
      uint32_t numBwps = nrHelper->GetNumberBwp (gnb);
      if (numBwps == 1)  // TDD
        {
          // Change the antenna orientation
          Ptr<NrGnbPhy> phy = nrHelper->GetGnbPhy (gnb, 0);
          Ptr<ThreeGppAntennaArrayModel> antenna =
              ConstCast<ThreeGppAntennaArrayModel> (phy->GetSpectrumPhy ()->GetAntennaArray());
          antenna->SetAttribute ("BearingAngle", DoubleValue (orientationRads));

          // Set numerology
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("Numerology", UintegerValue (ranHelper.GetNumerology ()));

          // Set TX power
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("TxPower", DoubleValue (10*log10 (x)));

          // Set TDD pattern
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("Pattern", StringValue (pattern));
        }

      else if (numBwps == 2)  //FDD
        {
          // Change the antenna orientation
          Ptr<NrGnbPhy> phy0 = nrHelper->GetGnbPhy (gnb, 0);
          Ptr<ThreeGppAntennaArrayModel> antenna0 =
              ConstCast<ThreeGppAntennaArrayModel> (phy0->GetSpectrumPhy ()->GetAntennaArray());
          antenna0->SetAttribute ("BearingAngle", DoubleValue (orientationRads));
          Ptr<NrGnbPhy> phy1 = nrHelper->GetGnbPhy (gnb, 1);
          Ptr<ThreeGppAntennaArrayModel> antenna1 =
              ConstCast<ThreeGppAntennaArrayModel> (phy1->GetSpectrumPhy ()->GetAntennaArray());
          antenna1->SetAttribute ("BearingAngle", DoubleValue (orientationRads));

          // Set numerology
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("Numerology", UintegerValue (ranHelper.GetNumerology ()));
          nrHelper->GetGnbPhy (gnb, 1)->SetAttribute ("Numerology", UintegerValue (ranHelper.GetNumerology ()));

          // Set TX power
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("TxPower", DoubleValue (10*log10 (x)));
          nrHelper->GetGnbPhy (gnb, 1)->SetAttribute ("TxPower", DoubleValue (-30.0));
          // Set TDD pattern
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("Pattern", StringValue ("DL|DL|DL|DL|DL|DL|DL|DL|DL|DL|"));
          nrHelper->GetGnbPhy (gnb, 1)->SetAttribute ("Pattern", StringValue ("UL|UL|UL|UL|UL|UL|UL|UL|UL|UL|"));

          // Link the two FDD BWP
          nrHelper->GetBwpManagerGnb (gnb)->SetOutputLink (1, 0);
        }

      else
        {
          NS_ABORT_MSG("Incorrect number of BWPs per CC");
        }
    }

  orientationRads = gridScenario.GetAntennaOrientationRadians (1, gridScenario.GetNumSectorsPerSite ());
  for (uint32_t numCell = 0; numCell < gnbSector2NetDev.GetN (); ++numCell)
    {
      Ptr<NetDevice> gnb = gnbSector2NetDev.Get (numCell);
      uint32_t numBwps = nrHelper->GetNumberBwp (gnb);
      if (numBwps == 1)  // TDD
        {
          // Change the antenna orientation
          Ptr<NrGnbPhy> phy = nrHelper->GetGnbPhy (gnb, 0);
          Ptr<ThreeGppAntennaArrayModel> antenna =
              ConstCast<ThreeGppAntennaArrayModel> (phy->GetSpectrumPhy ()->GetAntennaArray());
          antenna->SetAttribute ("BearingAngle", DoubleValue (orientationRads));

          // Set numerology
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("Numerology", UintegerValue (ranHelper.GetNumerology ()));

          // Set TX power
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("TxPower", DoubleValue (10*log10 (x)));

          // Set TDD pattern
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("Pattern", StringValue (pattern));
        }

      else if (numBwps == 2)  //FDD
        {
          // Change the antenna orientation
          Ptr<NrGnbPhy> phy0 = nrHelper->GetGnbPhy (gnb, 0);
          Ptr<ThreeGppAntennaArrayModel> antenna0 =
              ConstCast<ThreeGppAntennaArrayModel> (phy0->GetSpectrumPhy ()->GetAntennaArray());
          antenna0->SetAttribute ("BearingAngle", DoubleValue (orientationRads));
          Ptr<NrGnbPhy> phy1 = nrHelper->GetGnbPhy (gnb, 1);
          Ptr<ThreeGppAntennaArrayModel> antenna1 =
              ConstCast<ThreeGppAntennaArrayModel> (phy1->GetSpectrumPhy ()->GetAntennaArray());
          antenna1->SetAttribute ("BearingAngle", DoubleValue (orientationRads));

          // Set numerology
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("Numerology", UintegerValue (ranHelper.GetNumerology ()));
          nrHelper->GetGnbPhy (gnb, 1)->SetAttribute ("Numerology", UintegerValue (ranHelper.GetNumerology ()));

          // Set TX power
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("TxPower", DoubleValue (10*log10 (x)));
          nrHelper->GetGnbPhy (gnb, 1)->SetAttribute ("TxPower", DoubleValue (-30.0));

          // Set TDD pattern
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("Pattern", StringValue ("DL|DL|DL|DL|DL|DL|DL|DL|DL|DL|"));
          nrHelper->GetGnbPhy (gnb, 1)->SetAttribute ("Pattern", StringValue ("UL|UL|UL|UL|UL|UL|UL|UL|UL|UL|"));

          // Link the two FDD BWP
          nrHelper->GetBwpManagerGnb (gnb)->SetOutputLink (1, 0);
        }

      else
        {
          NS_ABORT_MSG("Incorrect number of BWPs per CC");
        }
    }

  orientationRads = gridScenario.GetAntennaOrientationRadians (2, gridScenario.GetNumSectorsPerSite ());
  for (uint32_t numCell = 0; numCell < gnbSector3NetDev.GetN (); ++numCell)
    {
      Ptr<NetDevice> gnb = gnbSector3NetDev.Get (numCell);
      uint32_t numBwps = nrHelper->GetNumberBwp (gnb);
      if (numBwps == 1)  // TDD
        {
          // Change the antenna orientation
          Ptr<NrGnbPhy> phy = nrHelper->GetGnbPhy (gnb, 0);
          Ptr<ThreeGppAntennaArrayModel> antenna =
              ConstCast<ThreeGppAntennaArrayModel> (phy->GetSpectrumPhy ()->GetAntennaArray());
          antenna->SetAttribute ("BearingAngle", DoubleValue (orientationRads));

          // Set numerology
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("Numerology", UintegerValue (ranHelper.GetNumerology ()));

          // Set TX power
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("TxPower", DoubleValue (10*log10 (x)));

          // Set TDD pattern
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("Pattern", StringValue (pattern));
        }

      else if (numBwps == 2)  //FDD
        {
          // Change the antenna orientation
          Ptr<NrGnbPhy> phy0 = nrHelper->GetGnbPhy (gnb, 0);
          Ptr<ThreeGppAntennaArrayModel> antenna0 =
              ConstCast<ThreeGppAntennaArrayModel> (phy0->GetSpectrumPhy ()->GetAntennaArray());
          antenna0->SetAttribute ("BearingAngle", DoubleValue (orientationRads));
          Ptr<NrGnbPhy> phy1 = nrHelper->GetGnbPhy (gnb, 1);
          Ptr<ThreeGppAntennaArrayModel> antenna1 =
              ConstCast<ThreeGppAntennaArrayModel> (phy1->GetSpectrumPhy ()->GetAntennaArray());
          antenna1->SetAttribute ("BearingAngle", DoubleValue (orientationRads));

          // Set numerology
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("Numerology", UintegerValue (ranHelper.GetNumerology ()));
          nrHelper->GetGnbPhy (gnb, 1)->SetAttribute ("Numerology", UintegerValue (ranHelper.GetNumerology ()));

          // Set TX power
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("TxPower", DoubleValue (10*log10 (x)));
          nrHelper->GetGnbPhy (gnb, 1)->SetAttribute ("TxPower", DoubleValue (-30.0));

          // Set TDD pattern
          nrHelper->GetGnbPhy (gnb, 0)->SetAttribute ("Pattern", StringValue ("DL|DL|DL|DL|DL|DL|DL|DL|DL|DL|"));
          nrHelper->GetGnbPhy (gnb, 1)->SetAttribute ("Pattern", StringValue ("UL|UL|UL|UL|UL|UL|UL|UL|UL|UL|"));

          // Link the two FDD BWP
          nrHelper->GetBwpManagerGnb (gnb)->SetOutputLink (1, 0);
        }

      else
        {
          NS_ABORT_MSG("Incorrect number of BWPs per CC");
        }
    }


  // Set the UE routing:

  if (operationMode == "FDD")
    {
      for (uint32_t i = 0; i < ueSector1NetDev.GetN (); i++)
        {
          nrHelper->GetBwpManagerUe (ueSector1NetDev.Get (i))->SetOutputLink (0, 1);
        }

      for (uint32_t i = 0; i < ueSector2NetDev.GetN (); i++)
        {
          nrHelper->GetBwpManagerUe (ueSector2NetDev.Get (i))->SetOutputLink (0, 1);
        }

      for (uint32_t i = 0; i < ueSector3NetDev.GetN (); i++)
        {
          nrHelper->GetBwpManagerUe (ueSector3NetDev.Get (i))->SetOutputLink (0, 1);
        }
    }

  for (uint32_t i = 0; i < ueSector1NetDev.GetN (); i++)
    {
      auto uePhyFirst = nrHelper->GetUePhy (ueSector1NetDev.Get (i), 0);
      uePhyFirst->TraceConnectWithoutContext ("ReportCurrentCellRsrpSinr",
                                              MakeBoundCallback (&ReportSinrNr, sinrStats));

      if (operationMode == "FDD")
        {
          auto uePhySecond = nrHelper->GetUePhy (ueSector1NetDev.Get (i), 1);
          uePhySecond->TraceConnectWithoutContext ("ReportPowerSpectralDensity",
                                                   MakeBoundCallback (&ReportPowerNr, powerStats));
        }
      else
        {
          uePhyFirst->TraceConnectWithoutContext ("ReportPowerSpectralDensity",
                                                  MakeBoundCallback (&ReportPowerNr, powerStats));
        }
    }

  for (uint32_t i = 0; i < ueSector2NetDev.GetN (); i++)
    {
      auto uePhyFirst = nrHelper->GetUePhy (ueSector2NetDev.Get (i), 0);
      uePhyFirst->TraceConnectWithoutContext ("ReportCurrentCellRsrpSinr",
                                              MakeBoundCallback (&ReportSinrNr, sinrStats));

      if (operationMode == "FDD")
        {
          auto uePhySecond = nrHelper->GetUePhy (ueSector2NetDev.Get (i), 1);
          uePhySecond->TraceConnectWithoutContext ("ReportPowerSpectralDensity",
                                                   MakeBoundCallback (&ReportPowerNr, powerStats));
        }
      else
        {
          uePhyFirst->TraceConnectWithoutContext ("ReportPowerSpectralDensity",
                                                  MakeBoundCallback (&ReportPowerNr, powerStats));
        }
    }

  for (uint32_t i = 0; i < ueSector3NetDev.GetN (); i++)
    {
      auto uePhyFirst = nrHelper->GetUePhy (ueSector3NetDev.Get (i), 0);
      uePhyFirst->TraceConnectWithoutContext ("ReportCurrentCellRsrpSinr",
                                              MakeBoundCallback (&ReportSinrNr, sinrStats));

      if (operationMode == "FDD")
        {
          auto uePhySecond = nrHelper->GetUePhy (ueSector3NetDev.Get (i), 1);
          uePhySecond->TraceConnectWithoutContext ("ReportPowerSpectralDensity",
                                                   MakeBoundCallback (&ReportPowerNr, powerStats));
        }
      else
        {
          uePhyFirst->TraceConnectWithoutContext ("ReportPowerSpectralDensity",
                                                  MakeBoundCallback (&ReportPowerNr, powerStats));
        }
    }

  // When all the configuration is done, explicitly call UpdateConfig ()

  for (auto it = gnbSector1NetDev.Begin (); it != gnbSector1NetDev.End (); ++it)
    {
      uint32_t bwpId = 0;
      if (operationMode == "FDD" && direction == "UL")
        {
          bwpId = 1;
        }
      auto gnbPhy = nrHelper->GetGnbPhy (*it, bwpId);
      gnbPhy->TraceConnectWithoutContext ("SlotDataStats",
                                          MakeBoundCallback (&ReportSlotStatsNr, slotStats));

      DynamicCast<NrGnbNetDevice> (*it)->UpdateConfig ();
    }

  for (auto it = gnbSector2NetDev.Begin (); it != gnbSector2NetDev.End (); ++it)
    {
      uint32_t bwpId = 0;
      if (operationMode == "FDD" && direction == "UL")
        {
          bwpId = 1;
        }
      auto gnbPhy = nrHelper->GetGnbPhy (*it, bwpId);
      gnbPhy->TraceConnectWithoutContext ("SlotDataStats",
                                          MakeBoundCallback (&ReportSlotStatsNr, slotStats));

      DynamicCast<NrGnbNetDevice> (*it)->UpdateConfig ();
    }

  for (auto it = gnbSector3NetDev.Begin (); it != gnbSector3NetDev.End (); ++it)
    {
      uint32_t bwpId = 0;
      if (operationMode == "FDD" && direction == "UL")
        {
          bwpId = 1;
        }
      auto gnbPhy = nrHelper->GetGnbPhy (*it, bwpId);
      gnbPhy->TraceConnectWithoutContext ("SlotDataStats",
                                          MakeBoundCallback (&ReportSlotStatsNr, slotStats));

      DynamicCast<NrGnbNetDevice> (*it)->UpdateConfig ();
    }

  for (auto it = ueSector1NetDev.Begin (); it != ueSector1NetDev.End (); ++it)
    {
      DynamicCast<NrUeNetDevice> (*it)->UpdateConfig ();
    }

  for (auto it = ueSector2NetDev.Begin (); it != ueSector2NetDev.End (); ++it)
    {
      DynamicCast<NrUeNetDevice> (*it)->UpdateConfig ();
    }

  for (auto it = ueSector3NetDev.Begin (); it != ueSector3NetDev.End (); ++it)
    {
      DynamicCast<NrUeNetDevice> (*it)->UpdateConfig ();
    }

}

static std::pair<ApplicationContainer, double>
InstallApps (const Ptr<Node> &ue, const Ptr<NetDevice> &ueDevice,
             const Address &ueAddress, const std::string &direction,
             UdpClientHelper *dlClientLowLat, const Ptr<Node> &remoteHost,
             const Ipv4Address &remoteHostAddr, uint32_t udpAppStartTimeMs,
             uint16_t dlPortLowLat, const Ptr<UniformRandomVariable> &x,
             uint32_t appGenerationTimeMs,
             const Ptr<LteHelper> &lteHelper, const Ptr<NrHelper> &nrHelper)
{
  ApplicationContainer app;

  // The bearer that will carry low latency traffic
  EpsBearer lowLatBearer (EpsBearer::NGBR_VIDEO_TCP_DEFAULT);

  // The filter for the low-latency traffic
  Ptr<EpcTft> lowLatTft = Create<EpcTft> ();
  EpcTft::PacketFilter dlpfLowLat;
  if (direction == "DL")
    {
      dlpfLowLat.localPortStart = dlPortLowLat;
      dlpfLowLat.localPortEnd = dlPortLowLat;
      dlpfLowLat.direction = EpcTft::DOWNLINK;
    }
  else
    {
      dlpfLowLat.remotePortStart = dlPortLowLat;
      dlpfLowLat.remotePortEnd = dlPortLowLat;
      dlpfLowLat.direction = EpcTft::UPLINK;
    }
  lowLatTft->Add (dlpfLowLat);

  // The client, who is transmitting, is installed in the remote host,
  // with destination address set to the address of the UE
  if (direction == "DL")
    {
      dlClientLowLat->SetAttribute ("RemoteAddress", AddressValue (ueAddress));
      app = dlClientLowLat->Install (remoteHost);
    }
  else
    {
      dlClientLowLat->SetAttribute ("RemoteAddress", AddressValue (remoteHostAddr));
      app = dlClientLowLat->Install (ue);
    }

  double startTime = x->GetValue (udpAppStartTimeMs, udpAppStartTimeMs + 10);
  app.Start (MilliSeconds (startTime));
  app.Stop (MilliSeconds (startTime + appGenerationTimeMs));

  std::cout << "\tStarts at time " << MilliSeconds (startTime).GetMilliSeconds () << " ms and ends at "
            << (MilliSeconds (startTime + appGenerationTimeMs)).GetMilliSeconds () << " ms" << std::endl;

  // Activate a dedicated bearer for the traffic type
  if (lteHelper != nullptr)
    {
      lteHelper->ActivateDedicatedEpsBearer (ueDevice, lowLatBearer, lowLatTft);
    }
  else if (nrHelper != nullptr)
    {
      nrHelper->ActivateDedicatedEpsBearer (ueDevice, lowLatBearer, lowLatTft);
    }
  else
    {
      NS_ABORT_MSG ("Programming error");
    }

  return std::make_pair(app, startTime);
}

int 
main (int argc, char *argv[])
{
  /*
   * Variables that represent the parameters we will accept as input by the
   * command line. Each of them is initialized with a default value.
   */
  // Scenario parameters (that we will use inside this script):
  uint16_t numOuterRings = 3;
  uint16_t ueNumPergNb = 2;
  bool logging = false;
  bool traces = true;
  std::string simulator = "";
  std::string scenario = "UMa";
  std::string radioNetwork = "NR";  // LTE or NR
  std::string operationMode = "TDD";  // TDD or FDD

  // Simulation parameters. Please don't use double to indicate seconds, use
  // milliseconds and integers to avoid representation errors.
  uint32_t appGenerationTimeMs = 1000;
  uint32_t udpAppStartTimeMs = 400;
  std::string direction = "DL";

  // Spectrum parameters. We will take the input from the command line, and then
  //  we will pass them inside the NR module.
  uint16_t numerologyBwp = 0;
  std::string pattern = "F|F|F|F|F|F|F|F|F|F|"; // Pattern can be e.g. "DL|S|UL|UL|DL|DL|S|UL|UL|DL|"
  uint32_t bandwidthMHz = 20;

  // Where we will store the output files.
  std::string simTag = "default";
  std::string outputDir = "./";

  // Error models
  std::string errorModel = "";

  bool calibration = true;

  uint32_t trafficScenario = 0;

  std::string scheduler = "PF";

  // Rem parameters: Modify them by hand, don't use the CommandLine for
  // the moment
  double xMinRem = -2000.0;
  double xMaxRem = 2000.0;
  uint16_t xResRem = 100;
  double yMinRem = -2000.0;
  double yMaxRem = 2000.0;
  uint16_t yResRem = 100;
  double zRem = 1.5;
  bool generateRem = false;
  uint32_t remSector = 1;


  /*
   * From here, we instruct the ns3::CommandLine class of all the input parameters
   * that we may accept as input, as well as their description, and the storage
   * variable.
   */
  CommandLine cmd;

  cmd.AddValue ("scenario",
                "The urban scenario string (UMa,UMi,RMa)",
                scenario);
  cmd.AddValue ("numRings",
                "The number of rings around the central site",
                numOuterRings);
  cmd.AddValue ("ueNumPergNb",
                "The number of UE per cell or gNB in multiple-ue topology",
                ueNumPergNb);
  cmd.AddValue ("appGenerationTimeMs",
                "Simulation time",
                appGenerationTimeMs);
  cmd.AddValue ("numerologyBwp",
                "The numerology to be used (NR only)",
                numerologyBwp);
  cmd.AddValue ("pattern",
                "The TDD pattern to use",
                pattern);
  cmd.AddValue ("direction",
                "The flow direction (DL or UL)",
                direction);
  cmd.AddValue ("simulator",
                "The cellular network simulator to use: LENA or 5GLENA",
                simulator);
  cmd.AddValue ("technology",
                "The radio access network technology",
                radioNetwork);
  cmd.AddValue ("operationMode",
                "The network operation mode can be TDD or FDD",
                operationMode);
  cmd.AddValue ("simTag",
                "tag to be appended to output filenames to distinguish simulation campaigns",
                simTag);
  cmd.AddValue ("outputDir",
                "directory where to store simulation results",
                outputDir);
  cmd.AddValue("errorModelType",
               "Error model type: ns3::NrEesmCcT1, ns3::NrEesmCcT2, ns3::NrEesmIrT1, ns3::NrEesmIrT2, ns3::NrLteMiErrorModel",
               errorModel);
  cmd.AddValue ("calibration",
                "disable a bunch of things to make LENA and NR_LTE comparable",
                calibration);
  cmd.AddValue ("trafficScenario",
                "0: saturation (110 Mbps/enb), 1: latency (1 pkt of 10 bytes), 2: low-load (20 Mbps)",
                trafficScenario);
  cmd.AddValue ("scheduler",
                "PF: Proportional Fair, RR: Round-Robin",
                scheduler);
  cmd.AddValue ("bandwidth",
                "BW in MHz for each BWP (integer value): valid values are 20, 10, 5",
                bandwidthMHz);

  // Parse the command line
  cmd.Parse (argc, argv);

  // Traffic parameters (that we will use inside this script:)
  uint32_t udpPacketSize = 1000;
  uint32_t lambda;
  uint32_t packetCount;

  NS_ABORT_MSG_IF (bandwidthMHz != 20 && bandwidthMHz != 10 && bandwidthMHz != 5,
                   "Valid bandwidth values are 20, 10, 5, you set " << bandwidthMHz);

  switch (trafficScenario)
    {
    case 0: // let's put 80 Mbps with 20 MHz of bandwidth. Everything else is scaled
      packetCount = 0xFFFFFFFF;
      switch (bandwidthMHz)
        {
        case 20:
          udpPacketSize = 1000;
          break;
        case 10:
          udpPacketSize = 500;
          break;
        case 5:
          udpPacketSize = 250;
          break;
        default:
          udpPacketSize = 1000;
        }
      lambda = 10000 / ueNumPergNb;
      break;
    case 1:
      packetCount = 1;
      udpPacketSize = 12;
      lambda = 1;
      break;
    case 2: // 20 Mbps == 2.5 MB/s in case of 20 MHz, everything else is scaled
      packetCount = 0xFFFFFFFF;
      switch (bandwidthMHz)
        {
        case 20:
          udpPacketSize = 250;
          break;
        case 10:
          udpPacketSize = 125;
          break;
        case 5:
          udpPacketSize = 75;
          break;
        default:
          udpPacketSize = 250;
        }
      lambda = 10000 / ueNumPergNb;
      break;
    default:
      NS_FATAL_ERROR ("Traffic scenario " << trafficScenario << " not valid. Valid values are 0 1 2");
    }

  SQLiteOutput db (outputDir + "/" + simTag + ".db", "lena-lte-comparison");
  SinrOutputStats sinrStats;
  PowerOutputStats powerStats;
  SlotOutputStats slotStats;

  sinrStats.SetDb (&db);
  powerStats.SetDb (&db);
  slotStats.SetDb (&db);

  /*
   * Check if the frequency and numerology are in the allowed range.
   * If you need to add other checks, here is the best position to put them.
   */
//  NS_ABORT_IF (centralFrequencyBand > 100e9);
  NS_ABORT_IF (numerologyBwp > 4);
  NS_ABORT_MSG_IF (direction != "DL" && direction != "UL", "Flow direction can only be DL or UL");
  NS_ABORT_MSG_IF (operationMode != "TDD" && operationMode != "FDD", "Operation mode can only be TDD or FDD");
  NS_ABORT_MSG_IF (radioNetwork != "LTE" && radioNetwork != "NR", "Unrecognized radio network technology");
  NS_ABORT_MSG_IF (simulator != "LENA" && simulator != "5GLENA", "Unrecognized simulator");
  NS_ABORT_MSG_IF (scheduler != "PF" && scheduler != "RR", "Unrecognized scheduler");
  /*
   * If the logging variable is set to true, enable the log of some components
   * through the code. The same effect can be obtained through the use
   * of the NS_LOG environment variable:
   *
   * export NS_LOG="UdpClient=level_info|prefix_time|prefix_func|prefix_node:UdpServer=..."
   *
   * Usually, the environment variable way is preferred, as it is more customizable,
   * and more expressive.
   */
  if (logging)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
      LogComponentEnable ("LtePdcp", LOG_LEVEL_INFO);
//      LogComponentEnable ("NrMacSchedulerOfdma", LOG_LEVEL_ALL);
    }

  /*
   * Default values for the simulation. We are progressively removing all
   * the instances of SetDefault, but we need it for legacy code (LTE)
   */
  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(999999999));

  /*
   * Create the scenario. In our examples, we heavily use helpers that setup
   * the gnbs and ue following a pre-defined pattern. Please have a look at the
   * HexagonalGridScenarioHelper documentation to see how the nodes will be distributed.
   */
  HexagonalGridScenarioHelper gridScenario;
  gridScenario.SetNumRings (numOuterRings);
  gridScenario.SetSectorization (HexagonalGridScenarioHelper::TRIPLE);
  gridScenario.SetScenarioParamenters (scenario);
  uint16_t gNbNum = gridScenario.GetNumCells ();
  uint32_t ueNum = ueNumPergNb * gNbNum;
  gridScenario.SetUtNumber (ueNum);
  gridScenario.CreateScenario ();  //!< Creates and plots the network deployment
  const uint16_t ffr = 3; // Fractional Frequency Reuse scheme to mitigate intra-site inter-sector interferences

  /*
   * Create different gNB NodeContainer for the different sectors.
   */
  NodeContainer gnbSector1Container, gnbSector2Container, gnbSector3Container;
  for (uint32_t j = 0; j < gridScenario.GetBaseStations ().GetN (); ++j)
    {
      Ptr<Node> gnb = gridScenario.GetBaseStations ().Get (j);
      switch (j % ffr)
      {
        case 0:
          gnbSector1Container.Add (gnb);
          break;
        case 1:
          gnbSector2Container.Add (gnb);
          break;
        case 2:
          gnbSector3Container.Add (gnb);
          break;
        default:
          NS_ABORT_MSG("ffr param cannot be larger than 3");
          break;
      }
    }

  /*
   * Create different UE NodeContainer for the different sectors.
   */
  NodeContainer ueSector1Container, ueSector2Container, ueSector3Container;

  for (uint32_t j = 0; j < gridScenario.GetUserTerminals ().GetN (); ++j)
    {
      Ptr<Node> ue = gridScenario.GetUserTerminals ().Get (j);
      switch (j % ffr)
      {
        case 0:
          ueSector1Container.Add (ue);
          break;
        case 1:
          ueSector2Container.Add (ue);
          break;
        case 2:
          ueSector3Container.Add (ue);
          break;
        default:
          NS_ABORT_MSG("ffr param cannot be larger than 3");
          break;
      }
    }

  /*
   * Setup the LTE or NR module. We create the various helpers needed inside
   * their respective configuration functions
   */
  Ptr<PointToPointEpcHelper> epcHelper;

  NetDeviceContainer gnbSector1NetDev, gnbSector2NetDev, gnbSector3NetDev;
  NetDeviceContainer ueSector1NetDev, ueSector2NetDev,ueSector3NetDev;

  Ptr <LteHelper> lteHelper = nullptr;
  Ptr <NrHelper> nrHelper = nullptr;

  if (simulator == "LENA")
    {
      epcHelper = CreateObject<PointToPointEpcHelper> ();
      SetLenaSimulatorParameters (gridScenario,
                                  scenario,
                                  gnbSector1Container,
                                  gnbSector2Container,
                                  gnbSector3Container,
                                  ueSector1Container,
                                  ueSector2Container,
                                  ueSector3Container,
                                  epcHelper,
                                  lteHelper,
                                  gnbSector1NetDev,
                                  gnbSector2NetDev,
                                  gnbSector3NetDev,
                                  ueSector1NetDev,
                                  ueSector2NetDev,
                                  ueSector3NetDev,
                                  calibration,
                                  &sinrStats,
                                  &powerStats,
                                  scheduler,
                                  bandwidthMHz);
    }
  else if (simulator == "5GLENA")
    {
      epcHelper = CreateObject<NrPointToPointEpcHelper> ();
      Set5gLenaSimulatorParameters (gridScenario,
                                    scenario,
                                    radioNetwork,
                                    errorModel,
                                    operationMode,
                                    direction,
                                    numerologyBwp,
                                    pattern,
                                    gnbSector1Container,
                                    gnbSector2Container,
                                    gnbSector3Container,
                                    ueSector1Container,
                                    ueSector2Container,
                                    ueSector3Container,
                                    epcHelper,
                                    nrHelper,
                                    gnbSector1NetDev,
                                    gnbSector2NetDev,
                                    gnbSector3NetDev,
                                    ueSector1NetDev,
                                    ueSector2NetDev,
                                    ueSector3NetDev,
                                    calibration,
                                    &sinrStats,
                                    &powerStats,
                                    &slotStats,
                                    scheduler,
                                    bandwidthMHz);
    }
  else
    {
      NS_ABORT_MSG ("Unrecognized cellular simulator");
    }

  // From here, it is standard NS3. In the future, we will create helpers
  // for this part as well.

  // create the internet and install the IP stack on the UEs
  // get SGW/PGW and create a single RemoteHost
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // connect a remoteHost to pgw. Setup routing too
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (2500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.000)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
  internet.Install (gridScenario.GetUserTerminals ());

  Ipv4InterfaceContainer ueSector1IpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueSector1NetDev));
  Ipv4InterfaceContainer ueSector2IpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueSector2NetDev));
  Ipv4InterfaceContainer ueSector3IpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueSector3NetDev));

  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  // Set the default gateway for the UEs
  for (uint32_t j = 0; j < gridScenario.GetUserTerminals ().GetN(); ++j)
    {
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (gridScenario.GetUserTerminals ().Get(j)->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // attach UEs to their gNB. Try to attach them per cellId order
  for (uint32_t u = 0; u < ueNum; ++u)
    {
      uint32_t sector = u % ffr;
      uint32_t i = u / ffr;
      if (sector == 0)
        {
          Ptr<NetDevice> gnbNetDev = gnbSector1NetDev.Get (i % gridScenario.GetNumSites ());
          Ptr<NetDevice> ueNetDev = ueSector1NetDev.Get (i);
          if (lteHelper != nullptr)
            {
              lteHelper->Attach (ueNetDev, gnbNetDev);
            }
          else if (nrHelper != nullptr)
            {
              nrHelper->AttachToEnb (ueNetDev, gnbNetDev);
            }
          else
            {
              NS_ABORT_MSG ("Programming error");
            }
          if (logging == true)
            {
              Vector gnbpos = gnbNetDev->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
              Vector uepos = ueNetDev->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
              double distance = CalculateDistance (gnbpos, uepos);
              std::cout << "Distance = " << distance << " meters" << std::endl;
            }
        }
      else if (sector == 1)
        {
          Ptr<NetDevice> gnbNetDev = gnbSector2NetDev.Get (i % gridScenario.GetNumSites ());
          Ptr<NetDevice> ueNetDev = ueSector2NetDev.Get (i);
          if (lteHelper != nullptr)
            {
              lteHelper->Attach (ueNetDev, gnbNetDev);
            }
          else if (nrHelper != nullptr)
            {
              nrHelper->AttachToEnb (ueNetDev, gnbNetDev);
            }
          else
            {
              NS_ABORT_MSG ("Programming error");
            }
          if (logging == true)
            {
              Vector gnbpos = gnbNetDev->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
              Vector uepos = ueNetDev->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
              double distance = CalculateDistance (gnbpos, uepos);
              std::cout << "Distance = " << distance << " meters" << std::endl;
            }
        }
      else if (sector == 2)
        {
          Ptr<NetDevice> gnbNetDev = gnbSector3NetDev.Get (i % gridScenario.GetNumSites ());
          Ptr<NetDevice> ueNetDev = ueSector3NetDev.Get (i);
          if (lteHelper != nullptr)
            {
              lteHelper->Attach (ueNetDev, gnbNetDev);
            }
          else if (nrHelper != nullptr)
            {
              nrHelper->AttachToEnb (ueNetDev, gnbNetDev);
            }
          else
            {
              NS_ABORT_MSG ("Programming error");
            }
          if (logging == true)
            {
              Vector gnbpos = gnbNetDev->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
              Vector uepos = ueNetDev->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
              double distance = CalculateDistance (gnbpos, uepos);
              std::cout << "Distance = " << distance << " meters" << std::endl;
            }
        }
      else
        {
          NS_ABORT_MSG("Number of sector cannot be larger than 3");
        }
    }

  /*
   * Traffic part. Install two kind of traffic: low-latency and voice, each
   * identified by a particular source port.
   */
  uint16_t dlPortLowLat = 1234;

  ApplicationContainer serverApps;

  // The sink will always listen to the specified ports
  UdpServerHelper dlPacketSinkLowLat (dlPortLowLat);

  // The server, that is the application which is listening, is installed in the UE
  if (direction == "DL")
    {
      serverApps.Add (dlPacketSinkLowLat.Install ({ueSector1Container,ueSector2Container,ueSector3Container}));
    }
  else
    {
      serverApps.Add (dlPacketSinkLowLat.Install (remoteHost));
    }

  // start UDP server
  serverApps.Start (MilliSeconds (udpAppStartTimeMs));

  /*
   * Configure attributes for the different generators, using user-provided
   * parameters for generating a CBR traffic
   *
   * Low-Latency configuration and object creation:
   */
  UdpClientHelper dlClientLowLat;
  dlClientLowLat.SetAttribute ("RemotePort", UintegerValue (dlPortLowLat));
  dlClientLowLat.SetAttribute ("MaxPackets", UintegerValue (packetCount));
  dlClientLowLat.SetAttribute ("PacketSize", UintegerValue (udpPacketSize));
  dlClientLowLat.SetAttribute ("Interval", TimeValue (Seconds (1.0/lambda)));

  /*
   * Let's install the applications!
   */
  ApplicationContainer clientApps;
  std::vector<NodeContainer*> nodes = { &ueSector1Container, &ueSector2Container, &ueSector3Container };
  std::vector<NetDeviceContainer*> devices = { &ueSector1NetDev, &ueSector2NetDev, &ueSector3NetDev };
  std::vector<Ipv4InterfaceContainer*> ips = { &ueSector1IpIface, &ueSector2IpIface, &ueSector3IpIface };

  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
  x->SetStream (RngSeedManager::GetRun());
  double maxStartTime = 0.0;

  for (uint32_t userId = 0; userId < gridScenario.GetUserTerminals().GetN (); ++userId)
    {
      for (uint32_t j = 0; j < 3; ++j)
        {
          if (nodes.at (j)->GetN () <= userId)
            {
              continue;
            }
          Ptr<Node> n = nodes.at(j)->Get (userId);
          Ptr<NetDevice> d = devices.at(j)->Get(userId);
          Address a = ips.at(j)->GetAddress(userId);

          std::cout << "app for ue " << userId << " in sector " << j+1
                    << " position " << n->GetObject<MobilityModel>()->GetPosition ()
                    << ":" << std::endl;

          auto app = InstallApps (n, d, a, direction, &dlClientLowLat, remoteHost,
                                  remoteHostAddr, udpAppStartTimeMs, dlPortLowLat,
                                  x, appGenerationTimeMs, lteHelper, nrHelper);
          maxStartTime = std::max (app.second, maxStartTime);
          clientApps.Add (app.first);
        }
    }

  // enable the traces provided by the nr module
  if (traces == true)
    {
      if (lteHelper != nullptr)
        {
          lteHelper->EnableTraces ();
        }
      else if (nrHelper != nullptr)
        {
          nrHelper->EnableTraces ();
        }
    }


  FlowMonitorHelper flowmonHelper;
  NodeContainer endpointNodes;
  endpointNodes.Add (remoteHost);
  endpointNodes.Add (gridScenario.GetUserTerminals ());

  Ptr<FlowMonitor> monitor = flowmonHelper.Install (endpointNodes);
  monitor->SetAttribute ("DelayBinWidth", DoubleValue (0.001));
  monitor->SetAttribute ("JitterBinWidth", DoubleValue (0.001));
  monitor->SetAttribute ("PacketSizeBinWidth", DoubleValue (20));

  std::string tableName = "e2e";


  Ptr<NrRadioEnvironmentMapHelper> remHelper; // Must be placed outside of block "if (generateRem)" because otherwise it gets destroyed,
                                              // and when simulation starts the object does not exist anymore, but the scheduled REM events do (exist).
                                              // So, REM events would be called with invalid pointer to remHelper ...
  if (generateRem)
    {
      if (simulator == "5GLENA" && !calibration)
        {
          NetDeviceContainer gnbContainerRem;
          Ptr<NetDevice> ueRemDevice;
          uint16_t remPhyIndex = 0;

          if (remSector == 1)
            {
              gnbContainerRem = gnbSector1NetDev;
              ueRemDevice = ueSector1NetDev.Get(0);
            }
          else if (remSector == 2)
            {
              gnbContainerRem = gnbSector2NetDev;
              ueRemDevice = ueSector2NetDev.Get(0);
            }
          else if (remSector == 3)
            {
              gnbContainerRem = gnbSector3NetDev;
              ueRemDevice = ueSector3NetDev.Get(0);
            }
          else
            {
              NS_FATAL_ERROR ("Sector does not exist");
            }

          //Radio Environment Map Generation for ccId 0
          remHelper = CreateObject<NrRadioEnvironmentMapHelper> ();
          remHelper->SetMinX (xMinRem);
          remHelper->SetMaxX (xMaxRem);
          remHelper->SetResX (xResRem);
          remHelper->SetMinY (yMinRem);
          remHelper->SetMaxY (yMaxRem);
          remHelper->SetResY (yResRem);
          remHelper->SetZ (zRem);

          //save beamforming vectors
          for (uint32_t j = 0; j < gridScenario.GetNumSites (); ++j)
            {
              switch (remSector - 1)
              {
                case 0:
                  gnbSector1NetDev.Get(j)->GetObject<NrGnbNetDevice>()->GetPhy(remPhyIndex)->GetBeamManager()->ChangeBeamformingVector(ueSector1NetDev.Get(j));
                  break;
                case 1:
                  gnbSector2NetDev.Get(j)->GetObject<NrGnbNetDevice>()->GetPhy(remPhyIndex)->GetBeamManager()->ChangeBeamformingVector(ueSector2NetDev.Get(j));
                  break;
                case 2:
                  gnbSector3NetDev.Get(j)->GetObject<NrGnbNetDevice>()->GetPhy(remPhyIndex)->GetBeamManager()->ChangeBeamformingVector(ueSector3NetDev.Get(j));
                  break;
                default:
                  NS_ABORT_MSG("sector cannot be larger than 3");
                  break;
              }
            }

          remHelper->CreateRem (gnbContainerRem, ueRemDevice, remPhyIndex);  //bwpId 0
        }
    }


  Simulator::Stop (MilliSeconds (appGenerationTimeMs + maxStartTime));
  Simulator::Run ();

  sinrStats.EmptyCache ();
  powerStats.EmptyCache ();
  slotStats.EmptyCache ();

  /*
   * To check what was installed in the memory, i.e., BWPs of eNb Device, and its configuration.
   * Example is: Node 1 -> Device 0 -> BandwidthPartMap -> {0,1} BWPs -> NrGnbPhy -> Numerology,
  GtkConfigStore config;
  config.ConfigureAttributes ();
  */

  FlowMonitorOutputStats flowMonStats;
  flowMonStats.SetDb (&db, tableName);
  flowMonStats.Save (monitor, flowmonHelper, outputDir + "/" + simTag);

  Simulator::Destroy ();
  return 0;
}


