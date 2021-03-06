/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2019 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#ifdef ARDUINO_ARCH_ESP32

#include "../../inc/MarlinConfigPre.h"

#if ENABLED(WIFISUPPORT)

#include "WebSocketSerial.h"
#include "wifi.h"
#include <ESPAsyncWebServer.h>

WebSocketSerial webSocketSerial;
AsyncWebSocket ws("/ws"); // TODO Move inside the class.

// RingBuffer impl

#define NEXT_INDEX(I, SIZE) ((I + 1) & (ring_buffer_pos_t)(SIZE - 1))

RingBuffer::RingBuffer(ring_buffer_pos_t size)
  : data(new uint8_t[size]),
    size(size),
    read_index(0),
    write_index(0)
{}

RingBuffer::~RingBuffer() { delete[] data; }

ring_buffer_pos_t RingBuffer::write(const uint8_t c) {
  const ring_buffer_pos_t n = NEXT_INDEX(write_index, size);

  if (n != read_index) {
    this->data[write_index] = c;
    write_index = n;
    return 1;
  }

  // TODO: buffer is full, handle?
  return 0;
}

ring_buffer_pos_t RingBuffer::write(const uint8_t *buffer, ring_buffer_pos_t size) {
  ring_buffer_pos_t written = 0;
  for (ring_buffer_pos_t i = 0; i < size; i++) {
    written += write(buffer[i]);
  }
  return written;
}

int RingBuffer::available(void) {
  return (size - read_index + write_index) & (size - 1);
}

int RingBuffer::peek(void) {
  return available() ? data[read_index] : -1;
}

int RingBuffer::read(void) {
  if (available()) {
    const int ret = data[read_index];
    read_index = NEXT_INDEX(read_index, size);
    return ret;
  }
  return -1;
}

ring_buffer_pos_t RingBuffer::read(uint8_t *buffer) {
   ring_buffer_pos_t len = available();

  for(ring_buffer_pos_t i = 0; read_index != write_index; i++) {
    buffer[i] = data[read_index];
    read_index = NEXT_INDEX(read_index, size);
  }

  return len;
}

void RingBuffer::flush(void) { read_index = write_index; }

// WebSocketSerial impl
WebSocketSerial::WebSocketSerial()
    : rx_buffer(RingBuffer(RX_BUFFER_SIZE)),
      tx_buffer(RingBuffer(TX_BUFFER_SIZE))
{}

void WebSocketSerial::begin(const long baud_setting) {
  ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
      case WS_EVT_CONNECT: client->ping(); break; // client connected
      case WS_EVT_DISCONNECT:                     // client disconnected
      case WS_EVT_ERROR:                          // error was received from the other end
      case WS_EVT_PONG: break;                    // pong message was received (in response to a ping request maybe)
      case WS_EVT_DATA: {                         // data packet
        AwsFrameInfo * info = (AwsFrameInfo*)arg;
        if (info->opcode == WS_TEXT || info->message_opcode == WS_TEXT)
          this->rx_buffer.write(data, len);
      }
    }
  });
  server.addHandler(&ws);
}

void WebSocketSerial::end() { }
int WebSocketSerial::peek(void) { return rx_buffer.peek(); }
int WebSocketSerial::read(void) { return rx_buffer.read(); }
int WebSocketSerial::available(void) { return rx_buffer.available(); }
void WebSocketSerial::flush(void) { rx_buffer.flush(); }

size_t WebSocketSerial::write(const uint8_t c) {
  size_t ret = tx_buffer.write(c);

  if (ret && c == '\n') {
    uint8_t tmp[TX_BUFFER_SIZE];
    ring_buffer_pos_t size = tx_buffer.read(tmp);
    ws.textAll(tmp, size);
  }

  return ret;
}

size_t WebSocketSerial::write(const uint8_t* buffer, size_t size) {
  size_t written = 0;
  for(size_t i = 0; i < size; i++) {
    written += write(buffer[i]);
  }
  return written;
}

void WebSocketSerial::flushTX(void) {
  // No need to do anything as there's no benefit to sending partial lines over the websocket connection.
}

#endif // WIFISUPPORT
#endif // ARDUINO_ARCH_ESP32
