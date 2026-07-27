#ifndef SERIAL_INPUT_H
#define SERIAL_INPUT_H

/* Opens a Windows COM port such as "COM5" at 115200 baud.
   Returns 1 on success and 0 on failure. */
int serial_input_open(const char *port_name);

/* Closes the serial port if it is open. */
void serial_input_close(void);

/* Non-blocking serial poll.
   Reads messages in the form HIT:0 through HIT:3 and stores the lane numbers
   in out_lanes. Returns the number of hits written, up to max_lanes. */
int serial_input_poll(int *out_lanes, int max_lanes);

/* Sends one command line to the ESP32, adding a newline automatically.
   Returns 1 if the complete command was written. */
int serial_input_send_command(const char *command);

/* Tells the ESP32 to flash a lane LED and play its hit sound. */
int serial_input_send_feedback(int lane);

/* Returns non-zero while the serial port is open. */
int serial_input_is_open(void);

#endif /* SERIAL_INPUT_H */
