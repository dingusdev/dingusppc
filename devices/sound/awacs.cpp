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

#include <core/timermanager.h>
#include <devices/deviceregistry.h>
#include <devices/sound/awacs.h>
#include <devices/sound/soundserver.h>
#include <devices/common/dbdma.h>
#include <endianswap.h>
#include <machines/machinebase.h>

#include <loguru.hpp>

AwacsBase::AwacsBase(std::string name) {
    supports_types(HWCompType::SND_CODEC);

    this->name = name;

    // connect to SoundServer
    this->snd_server = dynamic_cast<SoundServer *>
        (gMachineObj->get_comp_by_name("SoundServer"));
    this->out_stream_ready = false;
}

void AwacsBase::set_sample_rate(int sr_id) {
    if (sr_id > this->max_sr_id) {
        LOG_F(ERROR, "%s: invalid sample rate ID %d!", this->name.c_str(), sr_id);
    } else {
        this->cur_sample_rate = this->sr_table[sr_id];
    }
};

void AwacsBase::dma_out_start() {
    int     err;
    bool    reopen = false;

    if (this->out_stream_ready && this->out_sample_rate != this->cur_sample_rate)
        reopen = true;

    if (reopen) {
        snd_server->close_out_stream();
        this->out_stream_ready   = false;
        this->out_stream_running = false;
    }

    if (!this->out_stream_ready) {
        if ((err = this->snd_server->open_out_stream(this->cur_sample_rate,
            (void *)this->dma_out_ch))) {
            LOG_F(ERROR, "%s: unable to open sound output stream: %d",
                  this->name.c_str(), err);
            return;
        }

        this->out_sample_rate  = this->cur_sample_rate;
        this->out_stream_ready = true;
    }

    if (!this->out_stream_running) {
        if ((err = snd_server->start_out_stream())) {
            LOG_F(ERROR, "%s: could not start sound output stream: %d",
                  this->name.c_str(), err);
        }
    }
}

void AwacsBase::dma_out_stop() {
    if (this->out_stream_ready) {
        snd_server->close_out_stream();
        this->out_stream_ready   = false;
        this->out_stream_running = false;
    }
}

void AwacsBase::dma_out_pause() {
    this->out_stream_running = false;
}

static const char sound_input_data[2048] = {0};
static int sound_in_status = 0x10;

void AwacsBase::dma_in_data()
{
    // transfer data from sound input device
    this->dma_in_ch->push_data(sound_input_data, sizeof(sound_input_data));

    if (dma_in_ch->is_in_active()) {
        auto dbdma_ch = dynamic_cast<DMAChannel*>(dma_in_ch);
        sound_in_status <<= 1;
        if (!sound_in_status)
            sound_in_status = 1;
        dbdma_ch->set_stat(sound_in_status);
        LOG_F(INFO, "%s: status:%x", this->name.c_str(), sound_in_status);

        this->dma_in_timer_id = TimerManager::get_instance()->add_oneshot_timer(
            10000,
            [this]() {
                // re-enter the sequencer with the state specified in next_state
                this->dma_in_timer_id = 0;
                this->dma_in_data();
        });
    }
}

void AwacsBase::dma_in_start() {
    LOG_F(ERROR, "%s: dma_in_start", this->name.c_str());
    dma_in_data();
}

void AwacsBase::dma_in_stop() {
    if (this->dma_in_timer_id) {
        TimerManager::get_instance()->cancel_timer(this->dma_in_timer_id);
        this->dma_in_timer_id = 0;
    }
    LOG_F(ERROR, "%s: dma_in_stop", this->name.c_str());
}

void AwacsBase::dma_in_pause() {
    LOG_F(ERROR, "%s: dma_in_pause", this->name.c_str());
}

//=========================== PDM-style AWACs =================================
AwacDevicePdm::AwacDevicePdm() : AwacsBase("AWAC-PDM") {
    static int pdm_awac_freqs[3] = {22050, 29400, 44100};

    // PDM-style AWACs only supports three sample rates
    this->sr_table = pdm_awac_freqs;
    this->max_sr_id = 2;
}

uint32_t AwacDevicePdm::read_stat() {
    // TODO: implement all other status bits
    return (AWAC_REV_AWACS << 12) | (AWAC_MAKER_CRYSTAL << 8);
}

void AwacDevicePdm::write_ctrl(uint32_t addr, uint16_t value) {
    if (addr <= 4) {
        this->ctrl_regs[addr] = value;
    } else {
        LOG_F(ERROR, "%s: invalid control register address %d!", this->name.c_str(),
              addr);
    }
}

//============================= Screamer AWACs ================================
AwacsScreamer::AwacsScreamer(std::string name) : MacioSndCodec(name) {
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

uint32_t AwacsScreamer::snd_ctrl_read(uint32_t offset, int size) {
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

void AwacsScreamer::snd_ctrl_write(uint32_t offset, uint32_t value, int size) {
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
