/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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
*/

#include <devices/deviceregistry.h>
#include <devices/sound/awacs.h>
#include <devices/sound/soundserver.h>
#include <endianswap.h>
#include <machines/machinebase.h>

#include <loguru.hpp>

AwacsBase::AwacsBase(std::string name)
{
    supports_types(HWCompType::SND_CODEC);

    this->name = name;

    // connect to SoundServer
    this->snd_server = dynamic_cast<SoundServer *>
        (gMachineObj->get_comp_by_name("SoundServer"));
    this->out_stream_ready = false;
}

void AwacsBase::set_sample_rate(int sr_id)
{
    if (sr_id > this->max_sr_id) {
        LOG_F(ERROR, "%s: invalid sample rate ID %d!", this->name.c_str(), sr_id);
    } else {
        this->cur_sample_rate = this->sr_table[sr_id];
    }
};

void AwacsBase::dma_out_start()
{
    int     err;
    bool    reopen = false;

    if (this->out_stream_ready && this->out_sample_rate != this->cur_sample_rate)
        reopen = true;

    if (reopen) {
        snd_server->close_out_stream();
        this->out_stream_ready = false;
    }

    if (!this->out_stream_ready || reopen) {
        if ((err = this->snd_server->open_out_stream(this->cur_sample_rate,
            (void *)this->dma_out_ch))) {
            LOG_F(ERROR, "%s: unable to open sound output stream: %d",
                  this->name.c_str(), err);
            return;
        }

        if ((err = snd_server->start_out_stream())) {
            LOG_F(ERROR, "%s: could not start sound output stream", this->name.c_str());
        }

        this->out_sample_rate  = this->cur_sample_rate;
        this->out_stream_ready = true;
    }
}

void AwacsBase::dma_out_stop()
{
    if (this->out_stream_ready) {
        snd_server->close_out_stream();
        this->out_stream_ready = false;
    }
}

//=========================== PDM-style AWACs =================================
AwacDevicePdm::AwacDevicePdm() : AwacsBase("AWAC-PDM")
{
    static int pdm_awac_freqs[3] = {22050, 29400, 44100};

    // PDM-style AWACs only supports three sample rates
    this->sr_table = pdm_awac_freqs;
    this->max_sr_id = 2;
}

uint32_t AwacDevicePdm::read_stat()
{
    // TODO: implement all other status bits
    return (AWAC_REV_AWACS << 12) | (AWAC_MAKER_CRYSTAL << 8);
}

void AwacDevicePdm::write_ctrl(uint32_t addr, uint16_t value)
{
    if (addr <= 4) {
        this->ctrl_regs[addr] = value;
    } else {
        LOG_F(ERROR, "%s: invalid control register address %d!", this->name.c_str(),
              addr);
    }
}

//============================= Screamer AWACs ================================
AwacsScreamer::AwacsScreamer(std::string name) : MacioSndCodec(name)
{
    static int screamer_freqs[8] = {
        44100, 29400, 22050, 17640, 14700, 11025, 8820, 7350
    };

    this->sr_table = screamer_freqs;
    this->max_sr_id = 7;
}

int AwacsScreamer::device_postinit() {
    this->audio_proc = std::unique_ptr<AudioProcessor> (new AudioProcessor());

    /* register audio processor chip with the I2C bus */
    I2CBus* i2c_bus = dynamic_cast<I2CBus*>(gMachineObj->get_comp_by_type(HWCompType::I2C_HOST));
    i2c_bus->register_device(0x45, this->audio_proc.get());

    return 0;
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
        LOG_F(ERROR, "%s: unsupported register at offset 0x%X", this->name.c_str(), offset);
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
        this->set_sample_rate((this->snd_ctrl_reg >> 8) & 7);
        break;
    case AWAC_CODEC_CTRL_REG:
        subframe = (value >> 14) & 3;
        reg_num  = (value >> 20) & 7;
        data     = ((value >> 8) & 0xF00) | ((value >> 24) & 0xFF);
        LOG_F(9, "%s subframe = %d, reg = %d, data = %08X", this->name.c_str(),
              subframe, reg_num, data);
        if (!subframe)
            this->control_regs[reg_num] = data;
        break;
    default:
        LOG_F(ERROR, "%s: unsupported register at offset 0x%X", this->name.c_str(),
              offset);
    }
}

static const DeviceDescription Screamer_Descriptor = {
    AwacsScreamer::create, {}, {}
};

REGISTER_DEVICE(ScreamerSnd, Screamer_Descriptor);
