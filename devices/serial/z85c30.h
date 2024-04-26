/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
                      (theweirdo)     spatium

(Contact divingkatae#1017 or powermax#2286 on Discord for more info)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/** Register definitions for Z85C30. */

#ifndef Z85C30_H
#define Z85C30_H

enum
{
    WR0 = 0,
    WR1,
    WR2,
    WR3,
    WR4,
    WR5,
    WR6,
    WR7,
    WR8,
    WR9,
    WR10,
    WR11,
    WR12,
    WR13,
    WR14,
    WR15,
};

enum
{
    RR0 = 0,
    RR1,
    RR2,
    RR3,
    RR4,
    RR5,
    RR6,
    RR7,
    RR8,
    RR9,
    RR10,
    RR11,
    RR12,
    RR13,
    RR14,
    RR15,
};

enum
{
// RR0  - Transmit/Receive Buffer Status and External Status

    RR0_RX_CHARACTER_AVAILABLE                              = 1 << 0,
    RR0_ZERO_COUNT                                          = 1 << 1,
    RR0_TX_BUFFER_EMPTY                                     = 1 << 2,
    RR0_DCD                                                 = 1 << 3,
    RR0_SYNC_HUNT                                           = 1 << 4,
    RR0_CTS                                                 = 1 << 5,
    RR0_TX_UNDERRUN_EOM                                     = 1 << 6,
    RR0_BREAK_ABORT                                         = 1 << 7,

// RR1  - Special Receive Condition status, residue codes, error conditions

    RR1_ALL_SENT                                            = 1 << 0,
    RR1_RESIDUE_CODE_2                                      = 1 << 1,
    RR1_RESIDUE_CODE_1                                      = 1 << 2,
    RR1_RESIDUE_CODE_0                                      = 1 << 3,
    RR1_PARITY_ERROR                                        = 1 << 4,
    RR1_RX_OVERRUN_ERROR                                    = 1 << 5,
    RR1_CRC_FRAMING_ERROR                                   = 1 << 6,
    RR1_END_OF_FRAME_SDLC                                   = 1 << 7,

// RR2  - Modified (Channel B only) interrupt vector and Unmodified interrupt vector (Channel A only)

    RR2_V0                                                  = 1 << 0,
    RR2_V1                                                  = 1 << 1,
    RR2_V2                                                  = 1 << 2,
    RR2_V3                                                  = 1 << 3,
    RR2_V4                                                  = 1 << 4,
    RR2_V5                                                  = 1 << 5,
    RR2_V6                                                  = 1 << 6,
    RR2_V7                                                  = 1 << 7,

// RR3  - Interrupt Pending bits (Channel A only)

    RR3_CHANNEL_B_EXT_STAT_IP                               = 1 << 0,
    RR3_CHANNEL_B_TX_IP                                     = 1 << 1,
    RR3_CHANNEL_B_RX_IP                                     = 1 << 2,
    RR3_CHANNEL_A_EXT_STAT_IP                               = 1 << 3,
    RR3_CHANNEL_A_TX_IP                                     = 1 << 4,
    RR3_CHANNEL_A_RX_IP                                     = 1 << 5,
    // 0                                                    = 1 << 6,
    // 0                                                    = 1 << 7,

// RR6  - 14-bit frame byte count (LSB)

// RR7  - 14-bit frame byte count (MSB), frame status

    RR7_FIFO_OVERFLOW_STATUS                                = 1 << 7,
        RR7_NORMAL                                          = 0 << 7,
        RR7_FIFO_OVERFLOWED_DURING_OPERATION                = 1 << 7,
    RR7_FIFO_DATA_AVAILABLE_STATUS                          = 1 << 6,
        RR7_STATUS_READS_COME_FROM_SCC                      = 0 << 6,
        RR7_STATUS_READS_COME_FROM_10_X_9_BIT_FIFO          = 1 << 6,
    RR7_FRAME_BYTE_COUNT_MSB                                = 0x3F,

// RR8  - Receive Data // Receive buffer

// RR10 - Miscellaneous XMTR, RCVR status parameters

    // 0                                                    = 1 << 0,
    RR10_ON_LOOP                                            = 1 << 1,
    // 0                                                    = 1 << 2,
    // 0                                                    = 1 << 3,
    RR10_LOOP_SENDING                                       = 1 << 4,
    // 0                                                    = 1 << 5,
    RR10_TWO_CLOCKS_MISSING                                 = 1 << 6,
    RR10_ONE_CLOCK_MISSING                                  = 1 << 7,

// RR12 - Lower byte of baud rate generator time constant

// RR13 - Upper byte of baud rate generator time constant

// RR15 - External/Status interrupt control information

    RR15_SDLC_HDLC_ENHANCEMENT_STATUS                       = 1 << 0,
    RR15_ZERO_COUNT_IE                                      = 1 << 1,
    RR15_10_X_19_BIT_FRAME_STATUS_FIFO_ENABLE_STATUS        = 1 << 2,
    RR15_DCD_IE                                             = 1 << 3,
    RR15_SYNC_HUNT_IE                                       = 1 << 4,
    RR15_CTS_IE                                             = 1 << 5,
    RR15_TX_UNDERRUN_EOM_IE                                 = 1 << 6,
    RR15_BREAK_ABORT_IE                                     = 1 << 7,

// WR0  - Command Register // Command Register, (Register Pointers), CRC initialization, resets for various modes

    WR0_CRC_RESET_CODES                                     = 3 << 6,
        WR0_RESET_NULL_CODE                                 = 0 << 6,
        WR0_RESET_RX_CRC_CHECKER                            = 1 << 6,
        WR0_RESET_TX_CRC_GENERATOR                          = 2 << 6,
        WR0_RESET_TX_UNDERRUN_EOM_LATCH                     = 3 << 6,
    WR0_COMMAND_CODES                                       = 7 << 3,
        WR0_COMMAND_NULL_CODE                               = 0 << 3,
        WR0_COMMAND_POINT_HIGH                              = 1 << 3, // add 8 to Register Selection Code
        WR0_COMMAND_RESET_EXT_STATUS_INTERRUPTS             = 2 << 3,
        WR0_COMMAND_SEND_ABORT_SDLC                         = 3 << 3,
        WR0_COMMAND_ENABLE_INT_ON_NEXT_RX_CHARACTER         = 4 << 3,
        WR0_COMMAND_RESET_TXINT_PENDING                     = 5 << 3,
        WR0_COMMAND_ERROR_RESET                             = 6 << 3,
        WR0_COMMAND_RESET_HIGHEST_IUS                       = 7 << 3,
    WR0_REGISTER_SELECTION_CODE                             = 7 << 0,
        WR0_REGISTER_0                                      = 0 << 0,
        WR0_REGISTER_1                                      = 1 << 0,
        WR0_REGISTER_2                                      = 2 << 0,
        WR0_REGISTER_3                                      = 3 << 0,
        WR0_REGISTER_4                                      = 4 << 0,
        WR0_REGISTER_5                                      = 5 << 0,
        WR0_REGISTER_6                                      = 6 << 0,
        WR0_REGISTER_7                                      = 7 << 0,

// WR1  - Transmit/Receive Interrupt and Data Transfer Mode Definition // Interrupt conditions, Wait/DMA request control

    WR1_WAIT_DMA_REQUEST_ENABLE                             = 1 << 7,
    WR1_WAIT_DMA_REQUEST_FUNCTION                           = 1 << 6,
    WR1_WAIT_DMA_REQUEST_ON_RECEIVE_TRANSMIT                = 1 << 5,
    WR1_RECEIVE_INTERRUPT_MODES                             = 3 << 3,
        WR1_RX_INT_DISABLE                                  = 0 << 3,
        WR1_RX_INT_ON_FIRST_CHARACTER_OR_SPECIAL_CONDITION  = 1 << 3,
        WR1_INT_ON_ALL_RX_CHARACTERS_OR_SPECIAL_CONDITION   = 2 << 3,
        WR1_RX_INT_ON_SPECIAL_CONDITION_ONLY                = 3 << 3,
    WR1_PARITY_IS_SPECIAL_CONDITION                         = 1 << 2,
    WR1_TX_INT_ENABLE                                       = 1 << 1,
    WR1_EXT_INT_ENABLE                                      = 1 << 0,

// WR2  - Interrupt Vector // Interrupt vector (access through either channel)

    WR2_V0                                                  = 1 << 0,
    WR2_V1                                                  = 1 << 1,
    WR2_V2                                                  = 1 << 2,
    WR2_V3                                                  = 1 << 3,
    WR2_V4                                                  = 1 << 4,
    WR2_V5                                                  = 1 << 5,
    WR2_V6                                                  = 1 << 6,
    WR2_V7                                                  = 1 << 7,

// WR3  - Receive Parameters and Control // Receive/Control parameters, number of bits per character, Rx CRC enable

    WR3_RECEIVER_BITS_PER_CHARACTER                         = 3 << 6,
    WR3_BITS_PER_CHARACTER_5                                = 0 << 6,
    WR3_BITS_PER_CHARACTER_7                                = 1 << 6,
    WR3_BITS_PER_CHARACTER_6                                = 2 << 6,
    WR3_BITS_PER_CHARACTER_8                                = 3 << 6,
    WR3_AUTO_ENABLES                                        = 1 << 5,
    WR3_ENTER_HUNT_MODE                                     = 1 << 4,
    WR3_RX_CRC_ENABLE                                       = 1 << 3,
    WR3_ADDRESS_SEARCH_MODE_SDLC                            = 1 << 2,
    WR3_SYNC_CHARACTER_LOAD_INHIBIT                         = 1 << 1,
    WR3_RX_ENABLE                                           = 1 << 0,

// WR4  - Transmit/Receiver Miscellaneous Parameters and Modes // Transmit/Receive miscellaneous parameters and codes, clock rate, number of sync characters, stop bits, parity

    WR4_CLOCK_RATE                                          = 3 << 6,
        WR4_X1_CLOCK_MODE                                   = 0 << 6,
        WR4_X16_CLOCK_MODE                                  = 1 << 6,
        WR4_X32_CLOCK_MODE                                  = 2 << 6,
        WR4_X64_CLOCK_MODE                                  = 3 << 6,
    WR4_SYNC_MODE                                           = 3 << 4,
        WR4_MONOSYNC                                        = 0 << 4,
        WR4_BISYNC                                          = 1 << 4,
        WR4_SDLC_MODE                                       = 2 << 4,
        WR4_EXTERNAL_SYNC_MODE                              = 3 << 4,
    WR4_STOP_BITS                                           = 3 << 2,
        WR4_SYNC_MODES_ENABLE                               = 0 << 2,
        WR4_STOP_BITS_PER_CHARACTER_1                       = 1 << 2,
        WR4_STOP_BITS_PER_CHARACTER_1_AND_HALF              = 2 << 2,
        WR4_STOP_BITS_PER_CHARACTER_2                       = 3 << 2,
    WR4_PARITY                                              = 1 << 1,
        WR4_PARITY_ODD                                      = 0 << 1,
        WR4_PARITY_EVEN                                     = 1 << 1,
    WR4_PARITY_ENABLE                                       = 1 << 0,

// WR5  - Transmit Parameter and Controls // Transmit parameters and control, number of Tx bits per character, Tx CRC enable

    WR5_DTR                                                 = 1 << 7,
    WR5_TX_BITS_PER_CHARACTER                               = 3 << 5,
        WR5_TX_5_BITS_OR_LESS_PER_CHARACTER                 = 0 << 5,
        WR5_TX_7_BITS_PER_CHARACTER                         = 1 << 5,
        WR5_TX_6_BITS_PER_CHARACTER                         = 2 << 5,
        WR5_TX_8_BITS_PER_CHARACTER                         = 3 << 5,
    WR5_SEND_BREAK                                          = 1 << 4,
    WR5_TX_ENABLE                                           = 1 << 3,
    WR5_SDLC_CRC16                                          = 1 << 2,
    WR5_RTS                                                 = 1 << 1,
    WR5_TX_CRC_ENABLE                                       = 1 << 0,

// WR6  - Sync Characters or SDLC Address Field // Sync character (1st byte) or SDLC address

// WR7  - Sync Character or SDLCFlag/SDLC Option Register // SYNC character (2nd byte) or SDLC flag

// WR8  - Transmit buffer

// WR9  - Master Interrupt Control // Master interrupt control and reset (accessed through either channel), reset bits, control interrupt daisy chain

    WR9_RESET_COMMAND_BITS                                  = 3 << 6,
        WR9_NO_RESET                                        = 0 << 6,
        WR9_CHANNEL_RESET_B                                 = 1 << 6,
        WR9_CHANNEL_RESET_A                                 = 2 << 6,
        WR9_FORCE_HARDWARE_RESET                            = 3 << 6,
    WR9_INTERRUPT_CONTROL_BITS                              = 0x3F,
    WR9_INTERRUPT_MASKING_WITHOUT_INTACK                    = 1 << 5,
    WR9_STATUS_HIGH_STATUS_LOW                              = 1 << 4,
    WR9_MASTER_INTERRUPT_ENABLE                             = 1 << 3,
    WR9_DISABLE_LOWER_CHAIN                                 = 1 << 2,
    WR9_NO_VECTOR                                           = 1 << 1,
    WR9_VECTOR_INCLUDES_STATUS                              = 1 << 0,

// WR10 - Miscellaneous Transmitter/Receiver Control Bits // Miscellaneous transmitter/receiver control bits, NRZI, NRZ, FM encoding, CRC reset

    WR10_CRC_PRESET                                         = 1 << 7,
    WR10_DATA_ENCODING                                      = 3 << 5,
        WR10_NRZ                                            = 0 << 5,
        WR10_NRZI                                           = 1 << 5,
        WR10_FM1                                            = 2 << 5,
        WR10_FM0                                            = 3 << 5,
    WR10_GO_ACTIVE_ON_POLL                                  = 1 << 4,
    WR10_MARK_FLAG_IDLE                                     = 1 << 3,
        WR10_FLAG_IDLE                                      = 0 << 3,
        WR10_MARK_IDLE                                      = 1 << 3,
    WR10_ABORT_FLAG_ON_UNDERRUN                             = 1 << 2,
        WR10_FLAG_ON_UNDERRUN                               = 0 << 2,
        WR10_ABORT_ON_UNDERRUN                              = 1 << 2,
    WR10_LOOP_MODE                                          = 1 << 1,
    WR10_SYNC_SIZE                                          = 1 << 0,
        WR10_SYNC_6_BIT                                     = 0 << 0,
        WR10_SYNC_8_BIT                                     = 1 << 0,

// WR11 - Clock Mode Control // Clock mode control, source of Rx and Tx clocks

    WR11_RTXC_XTAL_NO_XTAL                                  = 1 << 7,
        WR11_RTXC_NO_XTAL                                   = 0 << 7,
        WR11_RTXC_XTAL                                      = 1 << 7,
    WR11_RECEIVER_CLOCK                                     = 3 << 5,
        WR11_RECEIVE_CLOCK_RTXC_PIN                         = 0 << 5,
        WR11_RECEIVE_CLOCK_TRXC_PIN                         = 1 << 5,
        WR11_RECEIVE_CLOCK_BR_GENERATOR_OUTPUT              = 2 << 5,
        WR11_RECEIVE_CLOCK_DPLL_OUTPUT                      = 3 << 5,
    WR11_TRANSMIT_CLOCK                                     = 3 << 3,
        WR11_TRANSMIT_CLOCK_RTXC_PIN                        = 0 << 3,
        WR11_TRANSMIT_CLOCK_TRXC_PIN                        = 1 << 3,
        WR11_TRANSMIT_CLOCK_BR_GENERATOR_OUTPUT             = 2 << 3,
        WR11_TRANSMIT_CLOCK_DPLL_OUTPUT                     = 3 << 3,
    WR11_TRXC_O_I                                           = 1 << 2,
    WR11_TRXC_OUTPUT_SOURCE                                 = 3 << 0,
        WR11_TRXC_OUT_XTAL_OUTPUT                           = 0 << 0,
        WR11_TRXC_OUT_TRANSMIT_CLOCK                        = 1 << 0,
        WR11_TRXC_OUT_BR_GENERATOR_OUTPUT                   = 2 << 0,
        WR11_TRXC_OUT_DPLL_OUTPUT                           = 3 << 0,

// WR12 - Lower Byte of Baud Rate Generator Time Constant

// WR13 - Upper Byte of Baud Rate Generator Time Constant

// WR14 - Miscellaneous Control Bits // Miscellaneous control bits: baud rate generator, Phase-Locked Loop control, auto echo, local loopback

    WR14_DPLL_COMMAND_BITS                                  = 7 << 5,
        WR14_DPLL_NULL_COMMAND                              = 0 << 5,
        WR14_DPLL_ENTER_SEARCH_MODE                         = 1 << 5,
        WR14_DPLL_RESET_MISSING_CLOCK                       = 2 << 5,
        WR14_DPLL_DISABLE_DPLL                              = 3 << 5,
        WR14_DPLL_SET_SOURCE_BR_GENERATOR                   = 4 << 5,
        WR14_DPLL_SET_SOURCE_RTXC                           = 5 << 5,
        WR14_DPLL_SET_FM_MODE                               = 6 << 5,
        WR14_DPLL_SET_NRZI_MODE                             = 7 << 5,
    WR14_LOCAL_LOOPBACK                                     = 1 << 4,
    WR14_AUTO_ECHO                                          = 1 << 3,
    WR14_DTR_REQUEST_FUNCTION                               = 1 << 2,
    WR14_BR_GENERATOR_SOURCE                                = 1 << 1,
    WR14_BR_GENERATOR_ENABLE                                = 1 << 0,

// WR15 - External/Status Interrupt Control // External/Status interrupt control information-control external conditions causing interrupts

    WR15_SDLC_HDLC_ENHANCEMENT_ENABLE                       = 1 << 0,
    WR15_ZERO_COUNT_IE                                      = 1 << 1,
    WR15_10_X_19_BIT_FRAME_STATUS_FIFO_ENABLE               = 1 << 2,
    WR15_DCD_IE                                             = 1 << 3,
    WR15_SYNC_HUNT_IE                                       = 1 << 4,
    WR15_CTS_IE                                             = 1 << 5,
    WR15_TX_UNDERRUN_EOM_IE                                 = 1 << 6,
    WR15_BREAK_ABORT_IE                                     = 1 << 7,
};

#endif // Z85C30_H
