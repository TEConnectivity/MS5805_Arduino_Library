#include <Wire.h>

#include "ms5805.h"

// Constants

// MS5805 device address
#define MS5805_ADDR 0x76 // 0b1110110

// MS5805 device commands
#define MS5805_RESET_COMMAND 0x1E
#define MS5805_START_PRESSURE_ADC_CONVERSION 0x40
#define MS5805_START_TEMPERATURE_ADC_CONVERSION 0x50
#define MS5805_READ_ADC 0x00

#define MS5805_CONVERSION_OSR_MASK 0x0F

// MS5805 commands
#define MS5805_PROM_ADDRESS_READ_ADDRESS_0 0xA0
#define MS5805_PROM_ADDRESS_READ_ADDRESS_1 0xA2
#define MS5805_PROM_ADDRESS_READ_ADDRESS_2 0xA4
#define MS5805_PROM_ADDRESS_READ_ADDRESS_3 0xA6
#define MS5805_PROM_ADDRESS_READ_ADDRESS_4 0xA8
#define MS5805_PROM_ADDRESS_READ_ADDRESS_5 0xAA
#define MS5805_PROM_ADDRESS_READ_ADDRESS_6 0xAC
#define MS5805_PROM_ADDRESS_READ_ADDRESS_7 0xAE

// Coefficients indexes for temperature and pressure computation
#define MS5805_CRC_INDEX 0
#define MS5805_PRESSURE_SENSITIVITY_INDEX 1
#define MS5805_PRESSURE_OFFSET_INDEX 2
#define MS5805_TEMP_COEFF_OF_PRESSURE_SENSITIVITY_INDEX 3
#define MS5805_TEMP_COEFF_OF_PRESSURE_OFFSET_INDEX 4
#define MS5805_REFERENCE_TEMPERATURE_INDEX 5
#define MS5805_TEMP_COEFF_OF_TEMPERATURE_INDEX 6

/**
* \brief Class constructor
*
*/
ms5805::ms5805(void) {}

/**
 * \brief Perform initial configuration. Has to be called once.
 */
void ms5805::begin(void) {
  Wire.begin();
}

/**
* \brief Check whether MS5805 device is connected
*
* \return bool : status of MS5805
*       - true : Device is present
*       - false : Device is not acknowledging I2C address
*/
boolean ms5805::is_connected(void) {
  Wire.beginTransmission((uint8_t)MS5805_ADDR);
  return (Wire.endTransmission() == 0);
}

/**
* \brief Writes the MS5805 8-bits command with the value passed
*
* \param[in] uint8_t : Command value to be written.
*
* \return ms5805_status : status of MS5805
*       - ms5805_status_ok : I2C transfer completed successfully
*       - ms5805_status_i2c_transfer_error : Problem with i2c transfer
*       - ms5805_status_no_i2c_acknowledge : I2C did not acknowledge
*/
enum ms5805_status ms5805::write_command(uint8_t cmd) {
  uint8_t i2c_status;

  Wire.beginTransmission((uint8_t)MS5805_ADDR);
  Wire.write(cmd);
  i2c_status = Wire.endTransmission();

  /* Do the transfer */
  if (i2c_status == ms5805_STATUS_ERR_OVERFLOW)
    return ms5805_status_no_i2c_acknowledge;
  if (i2c_status != ms5805_STATUS_OK)
    return ms5805_status_i2c_transfer_error;

  return ms5805_status_ok;
}

/**
* \brief Set  ADC resolution.
*
* \param[in] ms5805_resolution_osr : Resolution requested
*
*/
void ms5805::set_resolution(enum ms5805_resolution_osr res) {
  ms5805_resolution_osr = res;
}

/**
* \brief Reset the MS5805 device
*
* \return ms5805_status : status of MS5805
*       - ms5805_status_ok : I2C transfer completed successfully
*       - ms5805_status_i2c_transfer_error : Problem with i2c transfer
*       - ms5805_status_no_i2c_acknowledge : I2C did not acknowledge
*/
enum ms5805_status ms5805::reset(void) {
  return write_command(MS5805_RESET_COMMAND);
}

/**
* \brief Reads the ms5805 EEPROM coefficient stored at address provided.
*
* \param[in] uint8_t : Address of coefficient in EEPROM
* \param[out] uint16_t* : Value read in EEPROM
*
* \return ms5805_status : status of MS5805
*       - ms5805_status_ok : I2C transfer completed successfully
*       - ms5805_status_i2c_transfer_error : Problem with i2c transfer
*       - ms5805_status_no_i2c_acknowledge : I2C did not acknowledge
*       - ms5805_status_crc_error : CRC check error on the coefficients
*/
enum ms5805_status ms5805::read_eeprom_coeff(uint8_t command, uint16_t *coeff) {
  uint8_t buffer[2];
  uint8_t i;
  uint8_t i2c_status;

  buffer[0] = 0;
  buffer[1] = 0;

  /* Read data */
  Wire.beginTransmission((uint8_t)MS5805_ADDR);
  Wire.write(command);
  i2c_status = Wire.endTransmission();

  Wire.requestFrom((uint8_t)MS5805_ADDR, 2U);
  for (i = 0; i < 2; i++) {
    buffer[i] = Wire.read();
  }
  // Send the conversion command
  if (i2c_status == ms5805_STATUS_ERR_OVERFLOW)
    return ms5805_status_no_i2c_acknowledge;

  *coeff = (buffer[0] << 8) | buffer[1];

  return ms5805_status_ok;
}

/**
* \brief Reads the ms5805 EEPROM coefficients to store them for computation.
*
* \return ms5805_status : status of MS5805
*       - ms5805_status_ok : I2C transfer completed successfully
*       - ms5805_status_i2c_transfer_error : Problem with i2c transfer
*       - ms5805_status_no_i2c_acknowledge : I2C did not acknowledge
*       - ms5805_status_crc_error : CRC check error on the coefficients
*/
enum ms5805_status ms5805::read_eeprom(void) {
  enum ms5805_status status;
  uint8_t i;

  for (i = 0; i < MS5805_COEFFICIENT_COUNT; i++) {
    status = read_eeprom_coeff(MS5805_PROM_ADDRESS_READ_ADDRESS_0 + i * 2,
                               eeprom_coeff + i);
    if (status != ms5805_status_ok)
      return status;
  }
  if (!crc_check(eeprom_coeff, (eeprom_coeff[MS5805_CRC_INDEX] & 0xF000) >> 12))
    return ms5805_status_crc_error;

  coeff_read = true;

  return ms5805_status_ok;
}

/**
* \brief CRC check
*
* \param[in] uint16_t *: List of EEPROM coefficients
* \param[in] uint8_t : crc to compare with
*
* \return bool : TRUE if CRC is OK, FALSE if KO
*/
boolean ms5805::crc_check(uint16_t *n_prom, uint8_t crc) {
  uint8_t cnt, n_bit;
  uint16_t n_rem, crc_read;

  n_rem = 0x00;
  crc_read = n_prom[0];
  n_prom[MS5805_COEFFICIENT_COUNT] = 0;
  n_prom[0] = (0x0FFF & (n_prom[0])); // Clear the CRC byte

  for (cnt = 0; cnt < (MS5805_COEFFICIENT_COUNT + 1) * 2; cnt++) {

    // Get next byte
    if (cnt % 2 == 1)
      n_rem ^= n_prom[cnt >> 1] & 0x00FF;
    else
      n_rem ^= n_prom[cnt >> 1] >> 8;

    for (n_bit = 8; n_bit > 0; n_bit--) {

      if (n_rem & 0x8000)
        n_rem = (n_rem << 1) ^ 0x3000;
      else
        n_rem <<= 1;
    }
  }
  n_rem >>= 12;
  n_prom[0] = crc_read;

  return (n_rem == crc);
}

/**
* \brief Triggers conversion and read ADC value
*
* \param[in] uint8_t : Command used for conversion (will determine Temperature
* vs Pressure and osr)
* \param[out] uint32_t* : ADC value.
*
* \return ms5805_status : status of MS5805
*       - ms5805_status_ok : I2C transfer completed successfully
*       - ms5805_status_i2c_transfer_error : Problem with i2c transfer
*       - ms5805_status_no_i2c_acknowledge : I2C did not acknowledge
*/
enum ms5805_status ms5805::conversion_and_read_adc(uint8_t cmd, uint32_t *adc) {
  enum ms5805_status status;
  uint8_t i2c_status;
  uint8_t buffer[3];
  uint8_t i;

  /* Read data */
  Wire.beginTransmission((uint8_t)MS5805_ADDR);
  Wire.write((uint8_t)cmd);
  Wire.endTransmission();

  delay(conversion_time[(cmd & MS5805_CONVERSION_OSR_MASK) / 2]);

  Wire.beginTransmission((uint8_t)MS5805_ADDR);
  Wire.write((uint8_t)0x00);
  i2c_status = Wire.endTransmission();

  Wire.requestFrom((uint8_t)MS5805_ADDR, 3U);
  for (i = 0; i < 3; i++) {
    buffer[i] = Wire.read();
  }

  // delay conversion depending on resolution
  if (status != ms5805_status_ok)
    return status;

  // Send the read command
  // status = ms5805_write_command(MS5805_READ_ADC);
  if (status != ms5805_status_ok)
    return status;

  if (i2c_status == ms5805_STATUS_ERR_OVERFLOW)
    return ms5805_status_no_i2c_acknowledge;
  if (i2c_status != ms5805_STATUS_OK)
    return ms5805_status_i2c_transfer_error;

  *adc = ((uint32_t)buffer[0] << 16) | ((uint32_t)buffer[1] << 8) | buffer[2];

  return status;
}

/**
* \brief Reads the temperature and pressure ADC value and compute the
* compensated values.
*
* \param[out] float* : Celsius Degree temperature value
* \param[out] float* : mbar pressure value
*
* \return ms5805_status : status of MS5805
*       - ms5805_status_ok : I2C transfer completed successfully
*       - ms5805_status_i2c_transfer_error : Problem with i2c transfer
*       - ms5805_status_no_i2c_acknowledge : I2C did not acknowledge
*       - ms5805_status_crc_error : CRC check error on the coefficients
*/
enum ms5805_status ms5805::read_temperature_and_pressure(float *temperature,
                                                         float *pressure) {
  enum ms5805_status status = ms5805_status_ok;
  uint32_t adc_temperature, adc_pressure;
  int32_t dT, TEMP;
  int64_t OFF, SENS, P, T2, OFF2, SENS2;
  uint8_t cmd;

  // If first time adc is requested, get EEPROM coefficients
  if (coeff_read == false)
    status = read_eeprom();

  if (status != ms5805_status_ok)
    return status;

  // First read temperature
  cmd = ms5805_resolution_osr * 2;
  cmd |= MS5805_START_TEMPERATURE_ADC_CONVERSION;
  status = conversion_and_read_adc(cmd, &adc_temperature);
  if (status != ms5805_status_ok)
    return status;

  // Now read pressure
  cmd = ms5805_resolution_osr * 2;
  cmd |= MS5805_START_PRESSURE_ADC_CONVERSION;
  status = conversion_and_read_adc(cmd, &adc_pressure);
  if (status != ms5805_status_ok)
    return status;

  if (adc_temperature == 0 || adc_pressure == 0)
    return ms5805_status_i2c_transfer_error;

  // Difference between actual and reference temperature = D2 - Tref
  dT = (int32_t)adc_temperature -
       ((int32_t)eeprom_coeff[MS5805_REFERENCE_TEMPERATURE_INDEX] << 8);

  // Actual temperature = 2000 + dT * TEMPSENS
  TEMP = 2000 +
         ((int64_t)dT *
              (int64_t)eeprom_coeff[MS5805_TEMP_COEFF_OF_TEMPERATURE_INDEX] >>
          23);

  // Second order temperature compensation
  if (TEMP < 2000) {
    T2 = (3 * ((int64_t)dT * (int64_t)dT)) >> 33;
    OFF2 = 61 * ((int64_t)TEMP - 2000) * ((int64_t)TEMP - 2000) / 16;
    SENS2 = 29 * ((int64_t)TEMP - 2000) * ((int64_t)TEMP - 2000) / 16;

    if (TEMP < -1500) {
      OFF2 += 17 * ((int64_t)TEMP + 1500) * ((int64_t)TEMP + 1500);
      SENS2 += 9 * ((int64_t)TEMP + 1500) * ((int64_t)TEMP + 1500);
    }
  } else {
    T2 = (5 * ((int64_t)dT * (int64_t)dT)) >> 38;
    OFF2 = 0;
    SENS2 = 0;
  }

  // OFF = OFF_T1 + TCO * dT
  OFF = ((int64_t)(eeprom_coeff[MS5805_PRESSURE_OFFSET_INDEX]) << 17) +
        (((int64_t)(eeprom_coeff[MS5805_TEMP_COEFF_OF_PRESSURE_OFFSET_INDEX]) *
          dT) >>
         6);
  OFF -= OFF2;

  // Sensitivity at actual temperature = SENS_T1 + TCS * dT
  SENS =
      ((int64_t)eeprom_coeff[MS5805_PRESSURE_SENSITIVITY_INDEX] << 16) +
      (((int64_t)eeprom_coeff[MS5805_TEMP_COEFF_OF_PRESSURE_SENSITIVITY_INDEX] *
        dT) >>
       7);
  SENS -= SENS2;

  // Temperature compensated pressure = D1 * SENS - OFF
  P = (((adc_pressure * SENS) >> 21) - OFF) >> 15;

  *temperature = ((float)TEMP - T2) / 100;
  *pressure = (float)P / 100;

  return status;
}
