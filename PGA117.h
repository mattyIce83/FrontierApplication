#pragma once

/* PGA117 commands */

#define PGA117_CMD_READ          0x6a00
#define PGA117_CMD_WRITE         0x2a00
#define PGA117_CMD_NOOP          0x0000
#define PGA117_CMD_SDN_DIS       0xe100
#define PGA117_CMD_SDN_EN        0xe1f1

/* SPI Daisy-Chain Commands */

#define PGA117_DCCMD_SELECTOR    0x1000
#define PGA117_DCCMD_NOOP        (PGA117_DCCMD_SELECTOR | PGA117_CMD_NOOP)
#define PGA117_DCCMD_SDN_DIS     (PGA117_DCCMD_SELECTOR | PGA117_CMD_SDN_DIS)
#define PGA117_DCCMD_SDN_EN      (PGA117_DCCMD_SELECTOR | PGA117_CMD_SDN_EN)
#define PGA117_DCCMD_READ        (PGA117_DCCMD_SELECTOR | PGA117_CMD_READ)
#define PGA117_DCCMD_WRITE       (PGA117_DCCMD_SELECTOR | PGA117_CMD_WRITE)

/* PGA117 Commands **********************************************************/
/* Write command Gain Selection Bits
 *
 * the PGA117 provides scope gain selections (1, 2, 5, 10, 20, 50, 100, 200).
 */

#define PGA117_GAIN_1				(0)		/* Scope Gain 1   */
#define PGA117_GAIN_2				(1)		/* Scope Gain 2   */
#define PGA117_GAIN_5				(2)		/* Scope Gain 5   */
#define PGA117_GAIN_10				(3)		/* Scope Gain 10  */
#define PGA117_GAIN_20				(4)		/* Scope Gain 20  */
#define PGA117_GAIN_50				(5)		/* Scope Gain 50  */
#define PGA117_GAIN_100				(6)		/* Scope Gain 100 */
#define PGA117_GAIN_200				(7)		/* Scope Gain 200 */

/* Write command Mux Channel Selection Bits
 *
 * The PGA117 has a 10 channel input MUX.
 */

#define PGA117_CHANNEL_VCAL			(0)		/* VCAL/CH0 */
#define PGA117_CHANNEL_CH0			(0)		/* VCAL/CH0 */
#define PGA117_CHANNEL_CH1			(1)		/* CH1  */
#define PGA117_CHANNEL_CH2			(2)		/* CH2  */
#define PGA117_CHANNEL_CH3			(3)		/* CH3  */
#define PGA117_CHANNEL_CH4			(4)		/* CH4  */
#define PGA117_CHANNEL_CH5			(5)		/* CH5  */
#define PGA117_CHANNEL_CH6			(6)		/* CH6  */
#define PGA117_CHANNEL_CH7			(7)		/* CH7  */
#define PGA117_CHANNEL_CH8			(8)		/* CH8  */
#define PGA117_CHANNEL_CH9			(9)		/* CH9  */
#define PGA117_CHANNEL_CAL1			(12)	/* CAL1: connects to GND     */
#define PGA117_CHANNEL_CAL2			(13)	/* CAL2: connects to 0.9VCAL */
#define PGA117_CHANNEL_CAL3			(14)	/* CAL3: connects to 0.1VCAL */
#define PGA117_CHANNEL_CAL4			(15)	/* CAL4: connects to VREF    */

#define PGA117_GAIN_SHIFT			(4)		/* Bits 4-7: Gain Selection Bits */
#define PGA117_GAIN_MASK			(15 << PGA117_GAIN_SHIFT)

#define PGA117_CHANNEL_SHIFT		(0)		/* Bits 0-3: Channel Selection Bits */
#define PGA117_CHANNEL_MASK			(15 << PGA117_CHANNEL_SHIFT)


// For daisychained PGA117s, there is channel and gain for each PGA117
typedef struct
{
  struct
  {
    uint8_t channel;
    uint8_t gain;
  } mux1;

  struct
  {
    uint8_t channel;
    uint8_t gain;
  } mux2;
} PGA117_SETTINGS;


