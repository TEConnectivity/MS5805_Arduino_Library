#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#define MS5805_COEFFICIENT_COUNT 7

#define MS5805_CONVERSION_TIME_OSR_256 1
#define MS5805_CONVERSION_TIME_OSR_512 2
#define MS5805_CONVERSION_TIME_OSR_1024 3
#define MS5805_CONVERSION_TIME_OSR_2048 5
#define MS5805_CONVERSION_TIME_OSR_4096 9
#define MS5805_CONVERSION_TIME_OSR_8192 17

// Enum
enum ms5805_resolution_osr {
  ms5805_resolution_osr_256 = 0,
  ms5805_resolution_osr_512,
  ms5805_resolution_osr_1024,
  ms5805_resolution_osr_2048,
  ms5805_resolution_osr_4096,
  ms5805_resolution_osr_8192
};

enum ms5805_status {
  ms5805_status_ok,
  ms5805_status_no_i2c_acknowledge,
  ms5805_status_i2c_transfer_error,
  ms5805_status_crc_error
};

enum ms5805_status_code {
  ms5805_STATUS_OK = 0,
  ms5805_STATUS_ERR_OVERFLOW = 1,
  ms5805_STATUS_ERR_TIMEOUT = 4
};

// Functions
class ms5805 {

public:
  ms5805();

  /**
   * \brief Perform initial configuration. Has to be called once.
   */
  void begin();

  /**
  * \brief Check whether MS5805 device is connected
  *
  * \return bool : status of MS5805
  *       - true : Device is present
  *       - false : Device is not acknowledging I2C address
  */
  boolean is_connected(void);

  /**
  * \brief Reset the MS5805 device
  *
  * \return ms5805_status : status of MS5805
  *       - ms5805_status_ok : I2C transfer completed successfully
  *       - ms5805_status_i2c_transfer_error : Problem with i2c transfer
  *       - ms5805_status_no_i2c_acknowledge : I2C did not acknowledge
  */
  enum ms5805_status reset(void);

  /**
  * \brief Set  ADC resolution.
  *
  * \param[in] ms5805_resolution_osr : Resolution requested
  *
  */
  void set_resolution(enum ms5805_resolution_osr res);

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
  *       - ms5805_status_crc_error : CRC check error on on the PROM
  * coefficients
  */
  enum ms5805_status read_temperature_and_pressure(float *temperature,
                                                   float *pressure);

private:
  enum ms5805_status write_command(uint8_t cmd);
  enum ms5805_status read_eeprom_coeff(uint8_t command, uint16_t *coeff);
  boolean crc_check(uint16_t *n_prom, uint8_t crc);
  enum ms5805_status conversion_and_read_adc(uint8_t cmd, uint32_t *adc);
  enum ms5805_status read_eeprom(void);

  uint16_t eeprom_coeff[MS5805_COEFFICIENT_COUNT + 1];
  bool coeff_read = false;
  enum ms5805_status ms5805_write_command(uint8_t);
  enum ms5805_status ms5805_read_eeprom_coeff(uint8_t, uint16_t *);
  enum ms5805_status ms5805_read_eeprom(void);
  enum ms5805_status ms5805_conversion_and_read_adc(uint8_t, uint32_t *);

  enum ms5805_resolution_osr ms5805_resolution_osr;
  uint32_t conversion_time[6] = {
      MS5805_CONVERSION_TIME_OSR_256,  MS5805_CONVERSION_TIME_OSR_512,
      MS5805_CONVERSION_TIME_OSR_1024, MS5805_CONVERSION_TIME_OSR_2048,
      MS5805_CONVERSION_TIME_OSR_4096, MS5805_CONVERSION_TIME_OSR_8192};
};
