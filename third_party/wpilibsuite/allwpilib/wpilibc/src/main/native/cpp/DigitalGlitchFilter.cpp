/*----------------------------------------------------------------------------*/
/* Copyright (c) 2015-2018 FIRST. All Rights Reserved.                        */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#include "DigitalGlitchFilter.h"

#include <algorithm>
#include <array>

#include <HAL/Constants.h>
#include <HAL/DIO.h>
#include <HAL/HAL.h>

#include "Counter.h"
#include "Encoder.h"
#include "Utility.h"
#include "WPIErrors.h"

using namespace frc;

std::array<bool, 3> DigitalGlitchFilter::m_filterAllocated = {
    {false, false, false}};
wpi::mutex DigitalGlitchFilter::m_mutex;

DigitalGlitchFilter::DigitalGlitchFilter() {
  std::lock_guard<wpi::mutex> lock(m_mutex);
  auto index =
      std::find(m_filterAllocated.begin(), m_filterAllocated.end(), false);
  wpi_assert(index != m_filterAllocated.end());

  m_channelIndex = std::distance(m_filterAllocated.begin(), index);
  *index = true;

  HAL_Report(HALUsageReporting::kResourceType_DigitalFilter, m_channelIndex);
  SetName("DigitalGlitchFilter", m_channelIndex);
}

DigitalGlitchFilter::~DigitalGlitchFilter() {
  if (m_channelIndex >= 0) {
    std::lock_guard<wpi::mutex> lock(m_mutex);
    m_filterAllocated[m_channelIndex] = false;
  }
}

/**
 * Assigns the DigitalSource to this glitch filter.
 *
 * @param input The DigitalSource to add.
 */
void DigitalGlitchFilter::Add(DigitalSource* input) {
  DoAdd(input, m_channelIndex + 1);
}

void DigitalGlitchFilter::DoAdd(DigitalSource* input, int requestedIndex) {
  // Some sources from Counters and Encoders are null. By pushing the check
  // here, we catch the issue more generally.
  if (input) {
    // We don't support GlitchFilters on AnalogTriggers.
    if (input->IsAnalogTrigger()) {
      wpi_setErrorWithContext(
          -1, "Analog Triggers not supported for DigitalGlitchFilters");
      return;
    }
    int32_t status = 0;
    HAL_SetFilterSelect(input->GetPortHandleForRouting(), requestedIndex,
                        &status);
    wpi_setErrorWithContext(status, HAL_GetErrorMessage(status));

    // Validate that we set it correctly.
    int actualIndex =
        HAL_GetFilterSelect(input->GetPortHandleForRouting(), &status);
    wpi_assertEqual(actualIndex, requestedIndex);

    HAL_Report(HALUsageReporting::kResourceType_DigitalInput,
               input->GetChannel());
  }
}

/**
 * Assigns the Encoder to this glitch filter.
 *
 * @param input The Encoder to add.
 */
void DigitalGlitchFilter::Add(Encoder* input) {
  Add(input->m_aSource.get());
  if (StatusIsFatal()) {
    return;
  }
  Add(input->m_bSource.get());
}

/**
 * Assigns the Counter to this glitch filter.
 *
 * @param input The Counter to add.
 */
void DigitalGlitchFilter::Add(Counter* input) {
  Add(input->m_upSource.get());
  if (StatusIsFatal()) {
    return;
  }
  Add(input->m_downSource.get());
}

/**
 * Removes a digital input from this filter.
 *
 * Removes the DigitalSource from this glitch filter and re-assigns it to
 * the default filter.
 *
 * @param input The DigitalSource to remove.
 */
void DigitalGlitchFilter::Remove(DigitalSource* input) { DoAdd(input, 0); }

/**
 * Removes an encoder from this filter.
 *
 * Removes the Encoder from this glitch filter and re-assigns it to
 * the default filter.
 *
 * @param input The Encoder to remove.
 */
void DigitalGlitchFilter::Remove(Encoder* input) {
  Remove(input->m_aSource.get());
  if (StatusIsFatal()) {
    return;
  }
  Remove(input->m_bSource.get());
}

/**
 * Removes a counter from this filter.
 *
 * Removes the Counter from this glitch filter and re-assigns it to
 * the default filter.
 *
 * @param input The Counter to remove.
 */
void DigitalGlitchFilter::Remove(Counter* input) {
  Remove(input->m_upSource.get());
  if (StatusIsFatal()) {
    return;
  }
  Remove(input->m_downSource.get());
}

/**
 * Sets the number of cycles that the input must not change state for.
 *
 * @param fpgaCycles The number of FPGA cycles.
 */
void DigitalGlitchFilter::SetPeriodCycles(int fpgaCycles) {
  int32_t status = 0;
  HAL_SetFilterPeriod(m_channelIndex, fpgaCycles, &status);
  wpi_setErrorWithContext(status, HAL_GetErrorMessage(status));
}

/**
 * Sets the number of nanoseconds that the input must not change state for.
 *
 * @param nanoseconds The number of nanoseconds.
 */
void DigitalGlitchFilter::SetPeriodNanoSeconds(uint64_t nanoseconds) {
  int32_t status = 0;
  int fpgaCycles =
      nanoseconds * HAL_GetSystemClockTicksPerMicrosecond() / 4 / 1000;
  HAL_SetFilterPeriod(m_channelIndex, fpgaCycles, &status);

  wpi_setErrorWithContext(status, HAL_GetErrorMessage(status));
}

/**
 * Gets the number of cycles that the input must not change state for.
 *
 * @return The number of cycles.
 */
int DigitalGlitchFilter::GetPeriodCycles() {
  int32_t status = 0;
  int fpgaCycles = HAL_GetFilterPeriod(m_channelIndex, &status);

  wpi_setErrorWithContext(status, HAL_GetErrorMessage(status));

  return fpgaCycles;
}

/**
 * Gets the number of nanoseconds that the input must not change state for.
 *
 * @return The number of nanoseconds.
 */
uint64_t DigitalGlitchFilter::GetPeriodNanoSeconds() {
  int32_t status = 0;
  int fpgaCycles = HAL_GetFilterPeriod(m_channelIndex, &status);

  wpi_setErrorWithContext(status, HAL_GetErrorMessage(status));

  return static_cast<uint64_t>(fpgaCycles) * 1000L /
         static_cast<uint64_t>(HAL_GetSystemClockTicksPerMicrosecond() / 4);
}

void DigitalGlitchFilter::InitSendable(SendableBuilder&) {}