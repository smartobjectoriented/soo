#ifndef COMPANION_H
#define COMPANION_H


#include <linux/spi/spi.h>

#include <linux/gpio/consumer.h>
#include <linux/pwm.h>
#include <linux/i2c.h>

#include <xenomai/rtdm/driver.h>

/* Message bits and fields */
#define COMPANION_RW					7
#define COMPANION_ADDRESS				3
#define COMPANION_ADDRESS_MASK			0xf
#define COMPANION_DATA_MASK				0xff

#define DIGITAL_RST       0x00000001

#define COMM_SPI_READ    0x03
#define COMM_SPI_WRITE   0x02

#define ESC_WRITE 		   0x80
#define ESC_READ 		   0xC0

/* Addresses */
#define HW_CFG                  0x0074      /* hardware configuration register */
#define BYTE_TEST               0x0064      /* byte order test register */
#define RESET_CTL               0x01F8      /* reset register */
#define ID_REV                  0x0050      /* chip ID and revision */
#define IRQ_CFG                 0x0054      /* interrupt configuration */
#define INT_EN                  0x005C      /* interrupt enable */


#define ECAT_CSR_DATA           0x0300      /* EtherCAT CSR Interface Data Register */
#define ECAT_CSR_CMD            0x0304      /* EtherCAT CSR Interface Command Register */

#define ECAT_CSR_BUSY     0x80000000
#define PRAM_ABORT        0x40000000
#define PRAM_BUSY         0x80
#define PRAM_AVAIL        0x01
#define READY             0x08000000
#define DIGITAL_RST       0x00000001


#define ECAT_PRAM_RD_ADDR_LEN   0x0308      /* EtherCAT Process RAM Read Address and Length Register */
#define ECAT_PRAM_RD_CMD        0x030C      /* EtherCAT Process RAM Read Command Register */
#define ECAT_PRAM_WR_ADDR_LEN   0x0310      /* EtherCAT Process RAM Write Address and Length Register */
#define ECAT_PRAM_WR_CMD        0x0314      /* EtherCAT Process RAM Write Command Register */

#define ECAT_PRAM_RD_DATA       0x0000      /* EtherCAT Process RAM Read Data FIFO */
#define ECAT_PRAM_WR_DATA       0x0020      /* EtherCAT Process RAM Write Data FIFO */

#define RD_BUF_RX_LEN 0x4
#define RD_BUF_TX_LEN 0x3

#define WR_BUF_TX_LEN 0x7

#define EC_BYTE_PDO_NUM		32

/* EtherCAT registers */

#define EC_AL_STATUS               0x0130      /* AL status */
#define EC_WDOG_STATUS             0x0440      /* watch dog status */


/* EtherCAT state machine */
#define ESM_INIT                0x01          /* init */
#define ESM_PREOP               0x02          /* pre-operational */
#define ESM_BOOT                0x03          /* bootstrap */
#define ESM_SAFEOP              0x04          /* safe-operational */
#define ESM_OP                  0x08          /* operational */

/* Defines the number of step for our PWM.
 * For example a scale of 200 with a duty cycle of 50 is a 1/4 ON PWM. 	
 */
#define PWM_SCALE		200
#define NB_PWM			2


/* PDOs definitions for the out buffer */
#define PDO_OUTPUTS_GPIO 	0
#define PDO_DUTY_CYCLE_0	2
#define PDO_DUTY_CYCLE_1	3

/* PDOs definitions for the in buffer */
#define PDO_INPUTS_GPIO 	0

/**
 * IN and OUT PDOs buffers
 */
struct companion_buf {
	uint8_t in[EC_BYTE_PDO_NUM];
	uint8_t out[EC_BYTE_PDO_NUM];
};

/**
 * GPIOs used by the companion. Managed with the gpiod framework
 */
struct companion_gpios {
	struct gpio_desc *gpios_in[4];
	struct gpio_desc *gpios_out[4];
};


/**
 * Companion Status structure
 */
typedef struct companion_status  {
	struct companion_buf buffers;
	struct spi_device *spi;
	struct companion_gpios gpios;
	struct pwm_device *pwm[2];

	struct i2c_driver i2cdrv;
	struct i2c_client *i2c_client;

    struct gpio_desc *ec_irq_gpio;
    int ec_irq_no;
    rtdm_task_t rt_task;

    rtdm_irq_t irq_handle;
} companion_status_t ;


/* SI7021 specific (should be REMOVED int he final code)  */
#define SI7021_RESET_CMD        0xFE
#define SI7021_RD_USER_CMD      0xE7
#define SI7021_RD_FW1_CMD       0x84
#define SI7021_RD_FW2_CMD       0xB8
#define SI7021_MES_RD_TEMP_CMD  0xE3

#define SI7021_USR_DEFAULT_VAL  0x3A



int companion_register_i2c_driver(struct companion_status *companion);
int get_temperature(struct i2c_client *client);

#endif /* COMPANION_H */