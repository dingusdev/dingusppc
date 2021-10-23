/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-21 divingkatae and maximum
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

/** AWAC sound device emulation.

    Currently supported audio codecs:
    - PDM AWACs in Nubus Power Macintosh models
    - Screamer AWACs in Beige G3

    Author: Max Poliakovski 2019-21
*/

#include <devices/common/dbdma.h>
#include <devices/sound/awacs.h>
#include <devices/sound/soundserver.h>
#include <endianswap.h>
#include <machines/machinebase.h>

#include <algorithm>
#include <loguru.hpp>
#include <memory>

AwacsBase::AwacsBase()
{
    // connect to SoundServer
    this->snd_server = dynamic_cast<SoundServer *>
        (gMachineObj->get_comp_by_name("SoundServer"));
    this->out_stream_ready = false;
}

AwacsBase::~AwacsBase()
{
    // disconnect from SoundServer
    if (this->out_stream_ready) {
        snd_server->close_out_stream();
    }
}

void AwacsBase::set_sample_rate(int sr_id)
{
    if (sr_id > this->max_sr_id) {
        LOG_F(ERROR, "AWACs: invalid sample rate ID %d!", sr_id);
    } else {
        this->cur_sample_rate = this->sr_table[sr_id];
    }
};

void AwacsBase::start_output_dma()
{
    int err;

    if ((err = this->snd_server->open_out_stream(this->cur_sample_rate,
        (void *)this->dma_out_ch))) {
        LOG_F(ERROR, "AWACs: unable to open sound output stream: %d", err);
        this->out_stream_ready = false;
        return;
    }

    this->out_stream_ready = true;

    if ((err = snd_server->start_out_stream())) {
        LOG_F(ERROR, "AWACs: could not start sound output stream");
    }
}

void AwacsBase::stop_output_dma()
{
    if (this->out_stream_ready) {
        snd_server->close_out_stream();
        this->out_stream_ready = false;
    }
}

//=========================== PDM-style AWACs =================================
AwacDevicePdm::AwacDevicePdm() : AwacsBase()
{
    static int pdm_awac_freqs[3] = {22050, 29400, 44100};

    // PDM-style AWACs only supports three sample rates
    this->sr_table = pdm_awac_freqs;
    this->max_sr_id = 2;
}

uint32_t AwacDevicePdm::read_stat()
{
    LOG_F(INFO, "AWACs-PDM: status requested!");
    // TODO: return valid status including manufacturer & device IDs
    return 0;
}

void AwacDevicePdm::write_ctrl(uint32_t addr, uint16_t value)
{
    LOG_F(9, "AWACs-PDM: control updated, addr=0x%X, val=0x%X", addr, value);

    if (addr <= 4) {
        this->ctrl_regs[addr] = value;
    } else {
        LOG_F(ERROR, "AWACs-PDM: invalid control register address %d!", addr);
    }
}

//============================= Screamer AWACs ================================
AwacsScreamer::AwacsScreamer() : AwacsBase()
{
    this->audio_proc = std::unique_ptr<AudioProcessor> (new AudioProcessor());

    /* register audio processor chip with the I2C bus */
    I2CBus* i2c_bus = dynamic_cast<I2CBus*>(gMachineObj->get_comp_by_type(HWCompType::I2C_HOST));
    i2c_bus->register_device(0x45, this->audio_proc.get());

    static int screamer_freqs[8] = {
        44100, 29400, 22050, 17640, 14700, 11025, 8820, 7350
    };

    this->sr_table = screamer_freqs;
    this->max_sr_id = 7;
}

uint32_t AwacsScreamer::snd_ctrl_read(uint32_t offset, int size)
{
    switch (offset) {
    case AWAC_SOUND_CTRL_REG:
        return this->snd_ctrl_reg;
    case AWAC_CODEC_CTRL_REG:
        return this->is_busy;
    case AWAC_CODEC_STATUS_REG:
        return (AWAC_AVAILABLE << 8) | (AWAC_MAKER_CRYSTAL << 16) | (AWAC_REV_SCREAMER << 20);
    default:
        LOG_F(ERROR, "Screamer: unsupported register at offset 0x%X", offset);
    }

    return 0;
}

void AwacsScreamer::snd_ctrl_write(uint32_t offset, uint32_t value, int size)
{
    int subframe, reg_num;
    uint16_t data;

    switch (offset) {
    case AWAC_SOUND_CTRL_REG:
        this->snd_ctrl_reg = BYTESWAP_32(value);
        LOG_F(9, "Screamer: new sound control value = 0x%X", this->snd_ctrl_reg);
        break;
    case AWAC_CODEC_CTRL_REG:
        subframe = (value >> 14) & 3;
        reg_num  = (value >> 20) & 7;
        data     = ((value >> 8) & 0xF00) | ((value >> 24) & 0xFF);
        LOG_F(9, "Screamer subframe = %d, reg = %d, data = %08X\n",
              subframe, reg_num, data);
        if (!subframe)
            this->control_regs[reg_num] = data;
        break;
    default:
        LOG_F(ERROR, "Screamer: unsupported register at offset 0x%X", offset);
    }
}

void AwacsScreamer::open_stream(int sample_rate)
{
    int err;

    if ((err = this->snd_server->open_out_stream(sample_rate, (void *)this->dma_out_ch))) {
        LOG_F(ERROR, "Screamer: unable to open sound output stream: %d", err);
        this->out_stream_ready = false;
    } else {
        this->out_sample_rate = sample_rate;
        this->out_stream_ready = true;
    }
}

void AwacsScreamer::dma_start()
{
    int err;

    if (!this->out_stream_ready) {
        this->open_stream(this->sr_table[(this->snd_ctrl_reg >> 8) & 7]);
    } else if (this->out_sample_rate != this->sr_table[(this->snd_ctrl_reg >> 8) & 7]) {
        snd_server->close_out_stream();
        this->open_stream(this->sr_table[(this->snd_ctrl_reg >> 8) & 7]);
    } else {
        LOG_F(ERROR, "Screamer: unpausing attempted!");
        return;
    }

    if (!this->out_stream_ready) {
        return;
    }

    if ((err = snd_server->start_out_stream())) {
        LOG_F(ERROR, "Screamer: could not start sound output stream");
    }
}

void AwacsScreamer::dma_end()
{
}
