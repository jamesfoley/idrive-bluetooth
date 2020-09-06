#include <Arduino.h>
#include <ESP32CAN.h>
#include <CAN_config.h>
#include <BleGamepad.h>

BleGamepad bleGamepad;

CAN_device_t CAN_cfg;               // CAN Config
unsigned long previousMillis = 0;   // will store last time a CAN Message was send
const int heartbeat = 1000;          // interval at which send CAN Messages (milliseconds)
const int rx_queue_size = 10;       // Receive Queue size

char data[100];

const int button_count = 12;

char buttons[][14] = {
  "Menu",
  "Media",
  "Radio",
  "Tel",
  "Nav",
  "Back",
  "Option",
  "Rotary Middle",
  "Rotary Up",
  "Rotary Right",
  "Rotary Down",
  "Rotary Left"
};

int current_button_states[button_count];

int rotary_last_value = 0x00;

void setup() {
  Serial.begin(115200);

  // Configure CAN
  CAN_cfg.speed = CAN_SPEED_500KBPS;
  CAN_cfg.tx_pin_id = GPIO_NUM_5;
  CAN_cfg.rx_pin_id = GPIO_NUM_4;
  CAN_cfg.rx_queue = xQueueCreate(rx_queue_size, sizeof(CAN_frame_t));

  // Init CAN Module
  ESP32Can.CANInit();

  // Init bluetooth kb
  bleGamepad.begin();

  // Wait for ESP to be up
  delay(100);

  Serial.println("Basic Demo - ESP32-Arduino-CAN");

  for (int button = 0; button < button_count; button++) {
    current_button_states[button] = 0;
  }

  // Send wakeup twice
  CAN_frame_t tx_frame;
  tx_frame.FIR.B.FF = CAN_frame_std;
  tx_frame.MsgID = 0x202;
  tx_frame.FIR.B.DLC = 8;
  tx_frame.data.u8[0] = 0xFD;
  tx_frame.data.u8[1] = 0x00;
  tx_frame.data.u8[2] = 0x00;
  tx_frame.data.u8[3] = 0x00;
  tx_frame.data.u8[4] = 0x00;
  tx_frame.data.u8[5] = 0x00;
  tx_frame.data.u8[6] = 0x00;
  tx_frame.data.u8[7] = 0x00;
  ESP32Can.CANWriteFrame(&tx_frame);
  ESP32Can.CANWriteFrame(&tx_frame);
  Serial.println("Sent wakeup");

}

void loop() {

  // Define current millis
  unsigned long currentMillis = millis();

  // Define recieve frame
  CAN_frame_t rx_frame;

  // Receive next CAN frame from queue
  if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, 3 * portTICK_PERIOD_MS) == pdTRUE) {

    // Switch on receieved message ID
    switch (rx_frame.MsgID) {

      // Init
      case 0x5E7:
        if (rx_frame.data.u8[4] == 0x06) {
          // Send init reply
          CAN_frame_t tx_frame;
          tx_frame.FIR.B.FF = CAN_frame_std;
          tx_frame.MsgID = 0x273;
          tx_frame.FIR.B.DLC = 8;
          tx_frame.data.u8[0] = 0x1D;
          tx_frame.data.u8[1] = 0xE1;
          tx_frame.data.u8[2] = 0x00;
          tx_frame.data.u8[3] = 0xF0;
          tx_frame.data.u8[4] = 0xFF;
          tx_frame.data.u8[5] = 0x7F;
          tx_frame.data.u8[6] = 0xDE;
          tx_frame.data.u8[7] = rx_frame.data.u8[3];
          ESP32Can.CANWriteFrame(&tx_frame);
          Serial.println("Sent Init");
        }

        break;

      // Buttons
      case 0x267:
        button_press_event_handler(rx_frame);
        break;

      // Rotary
      case 0x264:
        rotary_event_handler(rx_frame);
        break;
    }

    //    printf("0x%08X, DLC %d, Data ", rx_frame.MsgID,  rx_frame.FIR.B.DLC);
    //    for (int i = 0; i < rx_frame.FIR.B.DLC; i++) {
    //      printf("0x%02X ", rx_frame.data.u8[i]);
    //    }
    //    printf("\n");
  }

  // Send heartbeat
  if (currentMillis - previousMillis >= heartbeat) {
    previousMillis = currentMillis;
    CAN_frame_t tx_frame;
    tx_frame.FIR.B.FF = CAN_frame_std;
    tx_frame.MsgID = 0x563;
    tx_frame.FIR.B.DLC = 8;
    tx_frame.data.u8[0] = 0x63;
    tx_frame.data.u8[1] = 0x00;
    tx_frame.data.u8[2] = 0x00;
    tx_frame.data.u8[3] = 0x00;
    tx_frame.data.u8[4] = 0x00;
    tx_frame.data.u8[5] = 0x00;
    tx_frame.data.u8[6] = 0x00;
    tx_frame.data.u8[7] = 0x00;
    ESP32Can.CANWriteFrame(&tx_frame);
  }
}

void button_press_event_handler (CAN_frame_t frame) {
  switch (frame.data.u8[4]) {
    // Normal button
    case 0xC0:
      switch (frame.data.u8[5]) {
        // Menu
        case 0x01:
          single_button_handler(0, frame.data.u8[3]);
          break;

        // Media
        case 0x10:
          single_button_handler(1, frame.data.u8[3]);
          break;

        // Radio
        case 0x08:
          single_button_handler(2, frame.data.u8[3]);
          break;

        // Tel
        case 0x40:
          single_button_handler(3, frame.data.u8[3]);
          break;

        // Nav
        case 0x20:
          single_button_handler(4, frame.data.u8[3]);
          break;

        // Back
        case 0x02:
          single_button_handler(5, frame.data.u8[3]);
          break;

        // Option
        case 0x04:
          single_button_handler(6, frame.data.u8[3]);
          break;
      }
      break;

    // Rotary button
    case 0xDE:

      // Rotary middle press and hold
      if (frame.data.u8[3] == 0x01 || frame.data.u8[3] == 0x02) {
        single_button_handler(7, 1);
      } else {
        single_button_handler(7, 0);
      }

      break;

    // Rotary direction
    case 0xDD:
      // Rotary up press and hold
      if (frame.data.u8[3] == 0x11 || frame.data.u8[3] == 0x12) {
        int state = frame.data.u8[3] == 0x11 ? 1 : 2;
        single_button_handler(8, state);

        // Rotary right press and hold
      } else if (frame.data.u8[3] == 0x21 || frame.data.u8[3] == 0x22) {
        int state = frame.data.u8[3] == 0x21 ? 1 : 2;
        single_button_handler(9, state);

        // Rotary down press and hold
      } else if (frame.data.u8[3] == 0x41 || frame.data.u8[3] == 0x42) {
        int state = frame.data.u8[3] == 0x41 ? 1 : 2;
        single_button_handler(10, state);

        // Rotary left press and hold
      } else if (frame.data.u8[3] == 0x81 || frame.data.u8[3] == 0x82) {
        int state = frame.data.u8[3] == 0x81 ? 1 : 2;
        single_button_handler(11, state);

      } else {
        if (current_button_states[8] != 0) {
          single_button_handler(8, 0);
        }
        if (current_button_states[9] != 0) {
          single_button_handler(9, 0);
        }
        if (current_button_states[10] != 0) {
          single_button_handler(10, 0);
        }
        if (current_button_states[11] != 0) {
          single_button_handler(11, 0);
        }
      }
      break;
  }

}

void single_button_handler(int button, int state) {
  // Get current state
  int current_state = current_button_states[button];

  // If state has changed, do something
  if (current_state != state) {
    sprintf(data, "Button state for %s changed to %d", buttons[button], state);
    Serial.println(data);

    if (bleGamepad.isConnected()) {
      switch (button) {
        case 0:
          if (state == 1) {
            bleGamepad.press(BUTTON_1);
          } else {
            bleGamepad.release(BUTTON_1);
          }
          break;
        case 1:
          if (state == 1) {
            bleGamepad.press(BUTTON_2);
          } else {
            bleGamepad.release(BUTTON_2);
          }
          break;
        case 2:
          if (state == 1) {
            bleGamepad.press(BUTTON_3);
          } else {
            bleGamepad.release(BUTTON_3);
          }
          break;
        case 3:
          if (state == 1) {
            bleGamepad.press(BUTTON_4);
          } else {
            bleGamepad.release(BUTTON_4);
          }
          break;
        case 4:
          if (state == 1) {
            bleGamepad.press(BUTTON_5);
          } else {
            bleGamepad.release(BUTTON_5);
          }
          break;
        case 5:
          if (state == 1) {
            bleGamepad.press(BUTTON_6);
          } else {
            bleGamepad.release(BUTTON_6);
          }
          break;
        case 6:
          if (state == 1) {
            bleGamepad.press(BUTTON_7);
          } else {
            bleGamepad.release(BUTTON_7);
          }
          break;
        case 7:
          if (state == 1) {
            bleGamepad.press(BUTTON_8);
          } else {
            bleGamepad.release(BUTTON_8);
          }
          break;
        case 8:
          if (state == 1) {
            bleGamepad.setAxes(0, -127, 0, 0, 0, 0, DPAD_CENTERED);
          } else {
            bleGamepad.setAxes(0, 0, 0, 0, 0, 0, DPAD_CENTERED);
          }
          break;

        case 9:
          if (state == 1) {
            bleGamepad.setAxes(127, 0, 0, 0, 0, 0, DPAD_CENTERED);
          } else {
            bleGamepad.setAxes(0, 0, 0, 0, 0, 0, DPAD_CENTERED);
          }
          break;

        case 10:
          if (state == 1) {
            bleGamepad.setAxes(0, 127, 0, 0, 0, 0, DPAD_CENTERED);
          } else {
            bleGamepad.setAxes(0, 0, 0, 0, 0, 0, DPAD_CENTERED);
          }
          break;

        case 11:
          if (state == 1) {
            bleGamepad.setAxes(-127, 0, 0, 0, 0, 0, DPAD_CENTERED);
          } else {
            bleGamepad.setAxes(0, 0, 0, 0, 0, 0, DPAD_CENTERED);
          }
          break;
      }
    }
  }

  // Make sure we keep track of current state
  current_button_states[button] = state;
}


void rotary_event_handler (CAN_frame_t frame) {
  int rotary_value = frame.data.u8[3];

  int difference = rotary_last_value - rotary_value;
  Serial.println(difference);

  if ((difference > 0 && difference < 20) || difference < -200) {
    rotary_turned_left();
  } else {
    rotary_turned_right();
  }

  // Set rotary value
  rotary_last_value = rotary_value;
}

void rotary_turned_left () {
  Serial.println("Rotary turned left");
  bleGamepad.press(BUTTON_9);
  delay(50);
  bleGamepad.release(BUTTON_9);
}

void rotary_turned_right () {
  Serial.println("Rotary turned right");
  bleGamepad.press(BUTTON_10);
  delay(50);
  bleGamepad.release(BUTTON_10);
}
