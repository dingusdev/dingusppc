/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-22 divingkatae and maximum
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

/** Character I/O backend implementations. */

#include <devices/serial/chario.h>
#include <loguru.hpp>

#include <cinttypes>
#include <cstring>
#include <memory>

bool CharIoNull::rcv_char_available()
{
    return false;
}

int CharIoNull::xmit_char(uint8_t c)
{
    return 0;
}

int CharIoNull::rcv_char(uint8_t *c)
{
    *c = 0xFF;
    return 0;
}

//======================== STDIO character I/O backend ========================
#ifdef _WIN32

#include <fcntl.h>
#include <io.h>
#include <windows.h>

HANDLE hInput  = GetStdHandle(STD_INPUT_HANDLE);
HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
DWORD old_in_mode, old_out_mode;
int old_stdin_trans_mode;

void mysig_handler(int signum) {
    SetStdHandle(signum, hInput);
    SetStdHandle(signum, hOutput);
}

int CharIoStdin::rcv_enable() {
    if (this->stdio_inited)
        return 0;

    GetConsoleMode(hInput, &old_in_mode);
    GetConsoleMode(hOutput, &old_out_mode);

    DWORD new_in_mode = old_in_mode;
    new_in_mode &= ~ENABLE_ECHO_INPUT;
    new_in_mode &= ~ENABLE_LINE_INPUT;
    new_in_mode &= ~ENABLE_PROCESSED_INPUT;

    new_in_mode |= ENABLE_EXTENDED_FLAGS;
    new_in_mode |= ENABLE_INSERT_MODE;
    new_in_mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;

    SetConsoleMode(hInput, new_in_mode);

    SetConsoleMode(hOutput, old_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    // disable automatic CRLF translation
    old_stdin_trans_mode = _setmode(_fileno(stdin), _O_BINARY);

    this->stdio_inited = true;

    LOG_F(INFO, "Winterm: receiver initialized");

    return 0;
}

void CharIoStdin::rcv_disable() {
    if (!this->stdio_inited)
        return;

    // restore original console mode
    SetConsoleMode(hInput, old_in_mode);
    SetConsoleMode(hOutput, old_out_mode);

    // restore original translation mode
    _setmode(_fileno(stdin), old_stdin_trans_mode);

    this->stdio_inited = false;

    LOG_F(INFO, "Winterm: receiver disabled");
}

bool CharIoStdin::rcv_char_available() {
    DWORD events;
    INPUT_RECORD buffer;

    PeekConsoleInput(hInput, &buffer, 1, &events);
    return !!(events > 0);
}

int CharIoStdin::xmit_char(uint8_t c) {
    _write(_fileno(stdout), &c, 1);
    return 0;
}

int CharIoStdin::rcv_char(uint8_t* c) {
    _read(_fileno(stdin), c, 1);
    return 0;
}

#else // non-Windows OS (Linux, mac OS etc.)

#include <signal.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

struct sigaction    old_act, new_act;
struct termios      orig_termios;

void mysig_handler(int signum)
{
    // restore original terminal state
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

    // restore original signal handler for SIGINT
    signal(SIGINT, old_act.sa_handler);

    LOG_F(INFO, "Old terminal state restored, SIG#=%d", signum);

    // re-post signal
    raise(signum);
}

int CharIoStdin::rcv_enable()
{
    if (this->stdio_inited)
        return 0;

    // save original terminal state
    tcgetattr(STDIN_FILENO, &orig_termios);

    struct termios new_termios = orig_termios;

    new_termios.c_cflag &= ~(CSIZE | PARENB);
    new_termios.c_cflag |= CS8;
    new_termios.c_lflag &= ~(ECHO | ICANON | ISIG);
    new_termios.c_iflag &= ~(ICRNL);

    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    // save original signal handler for SIGINT
    // then redirect SIGINT to new handler
    memset(&new_act, 0, sizeof(new_act));
    new_act.sa_handler = mysig_handler;
    sigaction(SIGINT, &new_act, &old_act);

    this->stdio_inited = true;

    return 0;
}

void CharIoStdin::rcv_disable()
{
    if (!this->stdio_inited)
        return;

    // restore original terminal state
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

    // restore original signal handler for SIGINT
    signal(SIGINT, old_act.sa_handler);

    this->stdio_inited = false;
}

bool CharIoStdin::rcv_char_available()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    fd_set savefds = readfds;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int chr;

    int sel_rv = select(1, &readfds, NULL, NULL, &timeout);
    return sel_rv > 0;
}

int CharIoStdin::xmit_char(uint8_t c)
{
    write(STDOUT_FILENO, &c, 1);
    return 0;
}

int CharIoStdin::rcv_char(uint8_t *c)
{
    read(STDIN_FILENO, c, 1);
    return 0;
}

#endif
