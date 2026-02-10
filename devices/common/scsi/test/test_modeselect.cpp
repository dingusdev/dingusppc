/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

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

/** @file Unit tests for SCSI MODE SELECT(6) parameter list parsing. */

#include <devices/common/scsi/scsiparseutils.h>

#include <cstring>
#include <iostream>
#include <string>

using namespace std;

static int ntested = 0;
static int nfailed = 0;

#define CHECK(cond, msg)                                            \
    do {                                                            \
        ntested++;                                                  \
        if (!(cond)) {                                              \
            nfailed++;                                              \
            cout << "FAIL: " << (msg)                               \
                 << " (" #cond ")" << endl;                         \
        }                                                           \
    } while (0)

// ---- Test: header-only, no block descriptor, no pages ----

static void test_header_only() {
    // 4-byte header: mode_data_len=0, medium_type=0, dev_param=0, bdl=0
    uint8_t data[] = { 0x00, 0x00, 0x00, 0x00 };

    ModeSelectData result;
    int rc = parse_mode_select_6(data, sizeof(data), nullptr, result);

    CHECK(rc == 0,           "header-only: should return 0 pages");
    CHECK(result.medium_type == 0,    "header-only: medium_type");
    CHECK(result.device_param == 0,   "header-only: device_param");
    CHECK(result.block_desc_len == 0, "header-only: block_desc_len");
    CHECK(result.num_pages == 0,      "header-only: num_pages");
}

// ---- Test: header + block descriptor, no pages ----

static void test_block_descriptor() {
    uint8_t data[] = {
        0x00, 0x00, 0x00, 0x08,     // header: bdl=8
        0x00,                        // density code
        0x00, 0x01, 0x00,            // num_blocks = 256
        0x00,                        // reserved
        0x00, 0x02, 0x00,            // block_len = 512
    };

    ModeSelectData result;
    int rc = parse_mode_select_6(data, sizeof(data), nullptr, result);

    CHECK(rc == 0,                      "block desc: should return 0 pages");
    CHECK(result.block_desc_len == 8,   "block desc: bdl");
    CHECK(result.bd_num_blocks == 256,  "block desc: num_blocks");
    CHECK(result.bd_block_len == 512,   "block desc: block_len");
}

// ---- Test: header + one mode page ----

static void test_single_page() {
    uint8_t data[] = {
        0x00, 0x00, 0x00, 0x00,     // header: no block descriptor
        0x01, 0x06,                  // page 0x01, length 6
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // page data
    };

    ModeSelectData result;
    int rc = parse_mode_select_6(data, sizeof(data), nullptr, result);

    CHECK(rc == 1,                       "single page: should return 1 page");
    CHECK(result.num_pages == 1,         "single page: num_pages");
    CHECK(result.pages[0].page_code == 0x01, "single page: page_code");
    CHECK(result.pages[0].page_len == 6,     "single page: page_len");
    CHECK(result.pages[0].data_offset == 6,  "single page: data_offset");
}

// ---- Test: header + block descriptor + multiple pages ----

static void test_multi_page() {
    uint8_t data[] = {
        0x00, 0x00, 0x00, 0x08,     // header: bdl=8
        0x00, 0x00, 0x00, 0x00,     // block desc (4 bytes)
        0x00, 0x00, 0x02, 0x00,     // block desc (4 bytes), block_len=512
        // page 0x01, length 6
        0x01, 0x06,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // page 0x03, length 2
        0x03, 0x02,
        0xAA, 0xBB,
        // page 0x30, length 4
        0x30, 0x04,
        0x41, 0x50, 0x50, 0x4C,    // "APPL"
    };

    ModeSelectData result;
    int rc = parse_mode_select_6(data, sizeof(data), nullptr, result);

    CHECK(rc == 3,                       "multi page: should return 3 pages");
    CHECK(result.pages[0].page_code == 0x01, "multi page: page[0] code");
    CHECK(result.pages[0].page_len == 6,     "multi page: page[0] len");
    CHECK(result.pages[1].page_code == 0x03, "multi page: page[1] code");
    CHECK(result.pages[1].page_len == 2,     "multi page: page[1] len");
    CHECK(result.pages[2].page_code == 0x30, "multi page: page[2] code");
    CHECK(result.pages[2].page_len == 4,     "multi page: page[2] len");
}

// ---- Test: PS (parameter saveable) bit in page code is masked ----

static void test_ps_bit_masked() {
    uint8_t data[] = {
        0x00, 0x00, 0x00, 0x00,     // header
        0x81, 0x02,                  // page_code byte = 0x81 → PS=1, code=0x01
        0x00, 0x00,
    };

    ModeSelectData result;
    int rc = parse_mode_select_6(data, sizeof(data), nullptr, result);

    CHECK(rc == 1,                       "ps bit: should return 1 page");
    CHECK(result.pages[0].page_code == 0x01, "ps bit: page_code masks to 0x01");
}

// ---- Test: too-short data (< 4 bytes) returns error ----

static void test_too_short() {
    uint8_t data[] = { 0x00, 0x00 };

    ModeSelectData result;
    int rc = parse_mode_select_6(data, sizeof(data), nullptr, result);

    CHECK(rc == -1, "too short: should return -1");
}

// ---- Test: block descriptor overflows data ----

static void test_bd_overflow() {
    // header says bdl=8, but only 6 bytes total
    uint8_t data[] = { 0x00, 0x00, 0x00, 0x08, 0x00, 0x00 };

    ModeSelectData result;
    int rc = parse_mode_select_6(data, sizeof(data), nullptr, result);

    CHECK(rc == -1, "bd overflow: should return -1");
}

// ---- Test: truncated page data returns error ----

static void test_truncated_page() {
    uint8_t data[] = {
        0x00, 0x00, 0x00, 0x00,     // header
        0x01, 0x06,                  // page 0x01, claims length 6
        0x00, 0x00,                  // but only 2 bytes of data follow
    };

    ModeSelectData result;
    int rc = parse_mode_select_6(data, sizeof(data), nullptr, result);

    CHECK(rc == -1, "truncated page: should return -1");
}

// ---- Test: medium type and device-specific parameter are captured ----

static void test_header_fields() {
    uint8_t data[] = {
        0x00, 0x05, 0x80, 0x00,     // medium_type=5, dev_param=0x80 (write protected)
    };

    ModeSelectData result;
    int rc = parse_mode_select_6(data, sizeof(data), nullptr, result);

    CHECK(rc == 0,                       "header fields: should return 0 pages");
    CHECK(result.medium_type == 0x05,    "header fields: medium_type");
    CHECK(result.device_param == 0x80,   "header fields: device_param (write protect)");
}

// ---- Test: data_offset points correctly into buffer ----

static void test_data_offset() {
    uint8_t data[] = {
        0x00, 0x00, 0x00, 0x08,     // header: bdl=8
        0x00, 0x00, 0x00, 0x00,     // block desc
        0x00, 0x00, 0x00, 0x00,     // block desc
        0x04, 0x03,                  // page 0x04, length 3
        0xDE, 0xAD, 0xBE,           // page data
    };

    ModeSelectData result;
    int rc = parse_mode_select_6(data, sizeof(data), nullptr, result);

    CHECK(rc == 1,                          "data offset: should return 1 page");
    CHECK(result.pages[0].data_offset == 14, "data offset: points past header+bd+page_hdr");
    CHECK(data[result.pages[0].data_offset] == 0xDE, "data offset: first byte is 0xDE");
}

// ---- Test: page data is accessible for memcpy via data_offset ----

static void test_page_data_copy() {
    uint8_t data[] = {
        0x00, 0x00, 0x00, 0x00,     // header
        0x01, 0x06,                  // page 0x01, length 6
        0x04, 0x03, 0x00, 0x00, 0x00, 0x00  // error recovery params
    };

    ModeSelectData result;
    int rc = parse_mode_select_6(data, sizeof(data), nullptr, result);
    CHECK(rc == 1, "page data copy: parse ok");

    // Simulate what the device does: copy page data into storage
    uint8_t storage[6] = {};
    int offset = result.pages[0].data_offset;
    int len = result.pages[0].page_len;
    CHECK(len == 6, "page data copy: page_len is 6");
    std::memcpy(storage, &data[offset], len < 6 ? len : 6);
    CHECK(storage[0] == 0x04, "page data copy: first byte copied");
    CHECK(storage[1] == 0x03, "page data copy: second byte copied");
}

// ---- Test: multiple pages with page data accessible ----

static void test_multi_page_data_access() {
    uint8_t data[] = {
        0x00, 0x00, 0x00, 0x00,     // header
        // page 0x01, length 2
        0x01, 0x02, 0xAA, 0xBB,
        // page 0x03, length 2
        0x03, 0x02, 0xCC, 0xDD,
    };

    ModeSelectData result;
    int rc = parse_mode_select_6(data, sizeof(data), nullptr, result);
    CHECK(rc == 2, "multi data access: 2 pages");

    CHECK(data[result.pages[0].data_offset] == 0xAA, "multi data access: page 0 data[0]");
    CHECK(data[result.pages[0].data_offset + 1] == 0xBB, "multi data access: page 0 data[1]");
    CHECK(data[result.pages[1].data_offset] == 0xCC, "multi data access: page 1 data[0]");
    CHECK(data[result.pages[1].data_offset + 1] == 0xDD, "multi data access: page 1 data[1]");
}

// ---- Test: zero-length page is valid ----

static void test_zero_length_page() {
    uint8_t data[] = {
        0x00, 0x00, 0x00, 0x00,     // header
        0x01, 0x00,                  // page 0x01, length 0 (empty page)
    };

    ModeSelectData result;
    int rc = parse_mode_select_6(data, sizeof(data), nullptr, result);
    CHECK(rc == 1,                       "zero len page: parse ok");
    CHECK(result.pages[0].page_code == 0x01, "zero len page: page_code");
    CHECK(result.pages[0].page_len == 0,     "zero len page: page_len is 0");
}

int main() {
    cout << "=== SCSI MODE SELECT(6) Parser Tests ===" << endl;

    test_header_only();
    test_block_descriptor();
    test_single_page();
    test_multi_page();
    test_ps_bit_masked();
    test_too_short();
    test_bd_overflow();
    test_truncated_page();
    test_header_fields();
    test_data_offset();
    test_page_data_copy();
    test_multi_page_data_access();
    test_zero_length_page();

    cout << endl;
    cout << ntested << " tests, " << nfailed << " failed." << endl;

    return nfailed ? 1 : 0;
}
