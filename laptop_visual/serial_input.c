#include "serial_input.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

static HANDLE serial_handle = INVALID_HANDLE_VALUE;
static char line_buffer[64];
static int line_length = 0;

int serial_input_open(const char *port_name) {
    if (!port_name || port_name[0] == '\0') return 0;

    serial_input_close();

    /* The \\.\ prefix also works for COM10 and higher. */
    char device_path[64];
    if (strncmp(port_name, "\\\\.\\", 4) == 0) {
        snprintf(device_path, sizeof(device_path), "%s", port_name);
    } else {
        snprintf(device_path, sizeof(device_path), "\\\\.\\%s", port_name);
    }

    serial_handle = CreateFileA(
        device_path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (serial_handle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Could not open serial port %s (Windows error %lu).\n",
                port_name, (unsigned long)GetLastError());
        return 0;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(serial_handle, &dcb)) {
        fprintf(stderr, "GetCommState failed (Windows error %lu).\n",
                (unsigned long)GetLastError());
        serial_input_close();
        return 0;
    }

    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;

    if (!SetCommState(serial_handle, &dcb)) {
        fprintf(stderr, "SetCommState failed (Windows error %lu).\n",
                (unsigned long)GetLastError());
        serial_input_close();
        return 0;
    }

    /* This combination makes ReadFile return immediately with whatever bytes
       are already available, so the SDL game loop is never blocked. */
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 50;

    if (!SetCommTimeouts(serial_handle, &timeouts)) {
        fprintf(stderr, "SetCommTimeouts failed (Windows error %lu).\n",
                (unsigned long)GetLastError());
        serial_input_close();
        return 0;
    }

    SetupComm(serial_handle, 4096, 4096);
    PurgeComm(serial_handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    line_length = 0;

    printf("Serial input connected on %s at 115200 baud.\n", port_name);
    fflush(stdout);
    return 1;
}

void serial_input_close(void) {
    if (serial_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(serial_handle);
        serial_handle = INVALID_HANDLE_VALUE;
    }
    line_length = 0;
}

int serial_input_is_open(void) {
    return serial_handle != INVALID_HANDLE_VALUE;
}

int serial_input_poll(int *out_lanes, int max_lanes) {
    if (!serial_input_is_open() || !out_lanes || max_lanes <= 0) return 0;

    int hit_count = 0;
    char read_buffer[128];

    while (hit_count < max_lanes) {
        DWORD bytes_read = 0;
        BOOL ok = ReadFile(
            serial_handle,
            read_buffer,
            (DWORD)sizeof(read_buffer),
            &bytes_read,
            NULL
        );

        if (!ok) {
            fprintf(stderr, "Serial read failed (Windows error %lu).\n",
                    (unsigned long)GetLastError());
            serial_input_close();
            break;
        }

        if (bytes_read == 0) break;

        for (DWORD i = 0; i < bytes_read; i++) {
            char ch = read_buffer[i];

            if (ch == '\r') continue;

            if (ch == '\n') {
                line_buffer[line_length] = '\0';

                int lane = -1;
                if (sscanf(line_buffer, "HIT:%d", &lane) == 1 &&
                    lane >= 0 && lane <= 3) {
                    out_lanes[hit_count++] = lane;
                    printf("FSR press: lane %d\n", lane);
                    fflush(stdout);

                    if (hit_count >= max_lanes) {
                        line_length = 0;
                        return hit_count;
                    }
                }

                line_length = 0;
            } else if (line_length < (int)sizeof(line_buffer) - 1) {
                line_buffer[line_length++] = ch;
            } else {
                /* Discard an unexpectedly long/corrupt line. */
                line_length = 0;
            }
        }
    }

    return hit_count;
}


int serial_input_send_command(const char *command) {
    if (!serial_input_is_open() || !command || command[0] == '\0') return 0;

    char output[128];
    int length = snprintf(output, sizeof(output), "%s\n", command);
    if (length <= 0 || length >= (int)sizeof(output)) {
        fprintf(stderr, "Serial command is too long.\n");
        return 0;
    }

    DWORD bytes_written = 0;
    BOOL ok = WriteFile(
        serial_handle,
        output,
        (DWORD)length,
        &bytes_written,
        NULL
    );

    if (!ok || bytes_written != (DWORD)length) {
        fprintf(stderr, "Serial write failed (Windows error %lu).\n",
                (unsigned long)GetLastError());
        return 0;
    }

    return 1;
}

int serial_input_send_feedback(int lane) {
    if (lane < 0 || lane > 3) return 0;

    char command[32];
    snprintf(command, sizeof(command), "FEEDBACK:%d", lane);
    return serial_input_send_command(command);
}
