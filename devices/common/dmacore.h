/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

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

/** @file Core definitions for direct memory access (DMA) emulation. */

#ifndef DMA_CORE_H
#define DMA_CORE_H

#include <cstdint>
#include <cinttypes>
#include <string>

enum DmaPullResult : int {
    MoreData,       // data source has more data to be pulled
    NoMoreData,     // data source has no more data to be pulled
};

enum DmaPushResult : int {
    NoData     = -1,  // data source is not running
    PushedData = 0,   // data was pushed
};

class DmaOutChannel {
public:
    DmaOutChannel(std::string name) { this->name = name; }

    virtual bool            is_out_active() { return true; }
    virtual DmaPullResult   pull_data(uint32_t req_len, uint32_t *avail_len,
                                      uint8_t **p_data) = 0;
    virtual int             get_pull_data_remaining() { return 1; }
    virtual void            end_pull_data() {}

    std::string get_name(void) { return this->name; }

private:
    std::string name;
};

class DmaInChannel {
public:
    DmaInChannel(std::string name) { this->name = name; }

    virtual bool            is_in_active() { return true; }
    virtual DmaPushResult   push_data(const char* src_ptr, int len) = 0;
    virtual int             get_push_data_remaining() { return 1; }
    virtual void            end_push_data() {}

    std::string get_name(void) { return this->name; }

private:
    std::string name;
};

// Base class for bidirectional DMA channels.
class DmaBidirChannel : public DmaOutChannel, public DmaInChannel {
public:
    DmaBidirChannel(std::string name) : DmaOutChannel(name + " Out"),
        DmaInChannel(name + std::string(" In")) { this->name = name; }

    std::string get_name(void) { return this->name; }

private:
    std::string name;
};

// ---------------------- New DMA API -------------------------
/** DMA channel types. */
enum DmaChannelType : unsigned {
    DMA_CH_TYPE_UNDEF = 0,
    DMA_CH_TYPE_IN,     // unidirectional channel, host <--- device
    DMA_CH_TYPE_OUT,    // unidirectional channel, host ---> device
    DMA_CH_TYPE_BIDIR,  // bidirectional channel,  host <--> device
};

/** DMA messages. */
enum DmaMsg : unsigned {
    DMA_MSG_START    = 1,
    DMA_MSG_STOP,
    DMA_MSG_FLUSH,
    DMA_MSG_DATA_AVAIL,
};

/** DMA transfer direction constants. */
enum XferDir : unsigned {
    DMA_DIR_UNDEF       = 0,
    DMA_DIR_TO_DEV,
    DMA_DIR_FROM_DEV,
};

class DmaChannel;

class DmaDevice {
public:
    DmaDevice()  = default;
    ~DmaDevice() = default;

    virtual void connect(DmaChannel *ch_obj) { this->channel_obj = ch_obj; }
    virtual void notify(DmaChannel *ch_obj, DmaMsg msg) {}
    virtual int  xfer_from(DmaChannel *ch_obj, uint8_t *buf, int len) { return len; }
    virtual int  xfer_to(DmaChannel *ch_obj, uint8_t *buf, int len) { return len; }
    virtual int  tell_xfer_size(DmaChannel *ch_obj) { return 0; }

protected:
    DmaChannel* channel_obj = nullptr;
};

class DmaChannel {
public:
    DmaChannel(const uint32_t id = 1) { this->ch_id = id; }
    DmaChannel(const DmaChannelType type, const uint32_t id = 1) {
        this->ch_type = type;
        this->ch_id   = id;
    }
    ~DmaChannel() = default;

    // setters/getters
    void        set_id(const uint32_t id) { this->ch_id = id; }
    uint32_t    get_id() { return this->ch_id; }
    void        set_type(const DmaChannelType type) { this->ch_type = type; }

    virtual void connect(DmaDevice *dev_obj) { this->dev_obj = dev_obj; }

    virtual void notify(DmaMsg msg) {}
    virtual bool dma_is_ready() { return false; }
    virtual void xfer_retry() {}

protected:
    DmaDevice*      dev_obj  = nullptr;
    uint32_t        ch_id    = 0; // support for several channels per device
    DmaChannelType  ch_type  = DMA_CH_TYPE_BIDIR;
    XferDir         xfer_dir = DMA_DIR_UNDEF;
};

/*
    TODO: write CONNECT macro that
    - constructs both DmaChannel and DmaDevice objects
    - calls connect() method on both objects
    - initializes DMA interrupts
*/

#endif // DMA_CORE_H
