// Non-blocking line or byte reader utilizing external buffer and buffer position. Version 1.1.2018.05.31.0// Defaults for optional parameters are to work in line reading mode. Cast output to (char*) and there you go.// params://   buffer: byte array pointer to store temporary data until a delimiter is found//   buffer_len: length of buffer array//   buffer_pos: int pointer to track and update the current position in the buffer array//   out: byte array pointer to copy data to when delimiter is detected//   out_len (opt): int pointer, returns length of data in output, excluding delimiter (which is also copied to out);             //   overflow_keep (opt): if false (default), will drop such data and all further data until delimiter is hit (i.e. ignoring too long data),//                        if true, will return data chunk (without delimiter) when buffer size is reached, no data is lost in this case;//                        you can use buffer_pos to detect overflow when overflow_keep = true://                        if (buffer_pos < 0) {} // overflow happened//   delim (opt): delimiter to look for, defaults to newline symbol '\n'//   delim_to_zero (opt): if true (default), will replace delimiter with 0 byte '\0'//                        if false, delimeter is copied to out as-is//   drop_special_byte (opt): if true (default), skip a specific byte value, if found//   special_byte (opt): which byte value to skip, works when drop_special_byte = true. Defaults to carriage return symbol '\r'.//   max_wait_millis (opt): after how many milliseconds to stop reading serial and return false;//                          this allows using the function in a loop, with low cost of each call,//                          accumulating data over time and eventually returning it when delimiter is found.//bool serial_readbytes_until(uint8_t buffer[], int buffer_len, int* buffer_pos, uint8_t out[], int* out_len = nullptr, bool overflow_keep = false,                            uint8_t delim = '\n', bool delim_to_zero = true, bool drop_special_byte = true, uint8_t special_byte = '\r',                            int max_wait_millis = 10) {    int32_t time = millis();  bool read = false;  if (*buffer_pos < 0) *buffer_pos = 0;  // resetting overflow indication  while (Serial.available() > 0) {    if (millis() - time > max_wait_millis && read) break;  // locking protection on high data inflow, but allow at least 1 byte to be read    if (*buffer_pos >= buffer_len) {  // too much data, buffer exhausted, need to decide what to do      if (overflow_keep) {  // returning accumulated data chunk without delimiter        memcpy(out, buffer, buffer_len);        *out_len = buffer_len;        *buffer_pos = -1;  // indicating overflow        return true;      } else {        uint8_t x = Serial.read();        if (x == delim) {*buffer_pos = 0;}  // dropping all accumulated data and all subsequent data until delimiter is found        continue;      }    }    uint8_t x = Serial.read();    if (drop_special_byte && x == special_byte) continue;    read = true;    buffer[*buffer_pos] = x;    if (x == delim) {  // found delimiter      if (delim_to_zero) buffer[*buffer_pos] = '\0';  // replacing delimiter with 0 byte      if (*buffer_pos > 0) {  // have data, returning result        memcpy(out, buffer, 1 + (*buffer_pos));        if (out_len != nullptr) *out_len = *buffer_pos;        *buffer_pos = 0;        return true;      } else {*buffer_pos = 0; continue;}  // there was no data before delimiter, skipping and resetting buffer position    }    (*buffer_pos)++;  }  return false;}#include <limits.h>#include <errno.h>// based on https://wiki.sei.cmu.edu/confluence/display/c/ERR34-C.+Detect+errors+when+converting+a+string+to+a+numberbool str_to_int(const char* buff, int* out_int) {  char *end;  errno = 0;  const long sl = strtol(buff, &end, 10);  if (end == buff) {    //fprintf(stderr, "%s: not a decimal number\n", buff);  } else if ('\0' != *end) {    //fprintf(stderr, "%s: extra characters at end of input: %s\n", buff, end);  } else if ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno) {    //fprintf(stderr, "%s out of range of type long\n", buff);  } else if (sl > INT_MAX) {    //fprintf(stderr, "%ld greater than INT_MAX\n", sl);  } else if (sl < INT_MIN) {    //fprintf(stderr, "%ld less than INT_MIN\n", sl);  } else {    *out_int = (int)sl;    return true;  }  return false;}