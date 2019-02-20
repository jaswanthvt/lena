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
#ifndef SRC_MMWAVE_MODEL_MMWAVE_AMC_H_
#define SRC_MMWAVE_MODEL_MMWAVE_AMC_H_

#include <ns3/mmwave-phy-mac-common.h>
#include <ns3/nr-error-model.h>

namespace ns3 {

/**
 * \ingroup error-models
 * \brief Adaptive Modulation and Coding class for the NR module
 *
 * The class has two option to calculate the CQI feedback (which is the MCS to use
 * in the future transmissions): the PIRO model or the "ErrorModel" model,
 * which uses the output of an error model to find the optimal MCS.
 *
 * Please note that it is necessary, even when using the PIRO model, to correctly
 * configure the ErrorModel type, which must be the same as the one set in the
 * MmWaveSpectrumPhy class.
 */
class NrAmc : public Object
{
public:
  /**
   * \brief GetTypeId
   * \return the TypeId of the Object
   */
  static TypeId GetTypeId (void);

  TypeId GetInstanceTypeId () const override;

  /**
   * \brief NrAmc constructor
   * \param ConfigParams PhyMacCommon params that will be tied with the instance
   */
  NrAmc (const Ptr<MmWavePhyMacCommon> & configParams);

  NrAmc () { }

  /**
   * \brief ~NrAmc deconstructor
   */
  virtual ~NrAmc ();

  /**
   * \brief Valid types of the model used to create a cqi feedback
   *
   * \see CreateCqiFeedbackWbTdma
   */
  enum AmcModel
  {
    PiroEW2010, //!< Piro version (very conservative)
    ErrorModel  //!< Error Model version (can use different error models, see NrErrorModel)
  };

  /**
   * \brief Get the MCS value from a CQI value
   * \param cqi the CQI
   * \return the MCS that corresponds to that CQI (it depends on the error model)
   */
  uint8_t GetMcsFromCqi (uint8_t cqi) const;

  /**
   * \brief Calculate the Payload Size (in bytes) from MCS and the number of RB
   * \param mcs MCS of the transmission
   * \param nprb Number of Physical Resource Blocks (not RBG)
   * \return the payload size in bytes
   */
  uint32_t GetPayloadSize (uint8_t mcs, uint32_t nprb) const;

  /**
   * \brief Calculate the TransportBlock size (in bytes) giving the MCS and the number of RB assigned
   *
   * It depends on the error model. Please note that this function expects in
   * input the RB, not the RBG of the transmission.
   *
   * \param mcs the MCS of the transmission
   * \param nprb The number of physical resource blocks used in the transmission
   * \return the TBS in bytes
   */
  uint32_t CalculateTbSize (uint8_t mcs, uint32_t nprb) const;

  /**
   * \brief Create a CQI/MCS wideband feedback from a SINR values
   *
   * \param sinr the sinr values
   * \param tbs the TBS (in byte)
   * \param mcsWb The calculated MCS
   * \return The calculated CQI
   */
  uint8_t CreateCqiFeedbackWbTdma (const SpectrumValue& sinr, uint32_t tbs, uint8_t &mcsWb);

  /**
   * \brief Get CQI from a SpectralEfficiency value
   * \param s spectral efficiency
   * \return the CQI (depends on the Error Model)
   */
  uint8_t GetCqiFromSpectralEfficiency (double s);

  /**
   * \brief Get MCS from a SpectralEfficiency value
   * \param s spectral efficiency
   * \return the MCS (depends on the Error Model)
   */
  uint8_t GetMcsFromSpectralEfficiency (double s);

 /**
  * \brief Get the maximum MCS (depends on the underlying error model)
  * \return the maximum MCS
  */
  uint32_t GetMaxMcs () const;

private:
  double m_ber;             //!< Piro model reference BER
  AmcModel m_amcModel;      //!< Type of the CQI feedback model
  Ptr<MmWavePhyMacCommon> m_phyMacConfig; //!< Pointer to PHY-MAC config
  Ptr<NrErrorModel> m_errorModel;         //!< Pointer to an instance of ErrorModel
  TypeId m_errorModelType;                //!< Type of the error model

  static const unsigned int m_crcLen = 24 / 8; //!< CRC length (in bytes)
};

} // end namespace ns3

#endif /* SRC_MMWAVE_MODEL_MMWAVE_AMC_H_ */
