#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "RadiationWatch.h"


/////////////////////////////////////////////////////////////
//
// CHANGE THESE SETTINGS!
//
const char* ssid     = "SSID_HERE";
const char* password  = "PASSWORD_HERE";
#define HOSTNAME "geiger1"
IPAddress ip( 192, 168, 1, 76 ); // hard coded IP
IPAddress gateway( 192, 168, 1, 1 ); // hard coded gateway
IPAddress subnet( 255, 255, 255, 0 );
const char* mqtt_server = "SERVER_IP_HERE";
////////////////////////////////////////////////////////////


const char* mqtt_topic_status = HOSTNAME"_status";
const char* mqtt_topic_in = HOSTNAME"_in";
const char* mqtt_topic_out = HOSTNAME"_out";
#define CONNECTION_ATTEMPT_NUM_TRIES  4
#define CONNECTION_ATTEMPT_DELAY      250 // ms
#define RESCAN_MAX_COUNT 15

WiFiClient EspClient;
PubSubClient client(EspClient);

// geiger config
int signPin = 12;
int noisePin = 13;
RadiationWatch radiationWatch( signPin, noisePin );

uint32_t calculateCRC32( const uint8_t *data, size_t length ) {
  uint32_t crc = 0xffffffff;
  while( length-- ) {
    uint8_t c = *data++;
    for( uint32_t i = 0x80; i > 0; i >>= 1 ) {
      bool bit = crc & 0x80000000;
      if( c & i ) {
        bit = !bit;
      }

      crc <<= 1;
      if( bit ) {
        crc ^= 0x04c11db7;
      }
    }
  }

  return crc;
}

// The ESP8266 RTC memory is arranged into blocks of 4 bytes. The access methods read and write 4 bytes at a time,
// so the RTC data structure should be padded to a 4-byte multiple.
struct _rtcData {
  uint32_t crc32;   // 4 bytes
  uint8_t channel;  // 1 byte,   5 in total
  uint8_t bssid[6]; // 6 bytes, 11 in total
  uint8_t padding;  // 1 byte,  12 in total
};
struct _rtcData rtcData = {};
bool rtcValid = false;

void sleepSave( void ) {
  // Write current connection info back to RTC
  rtcData.channel = WiFi.channel();
  memcpy( rtcData.bssid, WiFi.BSSID(), 6 ); // Copy 6 bytes of BSSID (AP's MAC address)
  rtcData.crc32 = calculateCRC32( ((uint8_t*)&rtcData) + 4, sizeof( rtcData ) - 4 );
  ESP.rtcUserMemoryWrite( 0, (uint32_t*)&rtcData, sizeof( rtcData ) );
}
void sleepResume( void ) {
  // Try to read WiFi settings from RTC memory
  rtcValid = false;
  if( ESP.rtcUserMemoryRead( 0, (uint32_t*)&rtcData, sizeof( rtcData ) ) ) {
    // Calculate the CRC of what we just read from RTC memory, but skip the first 4 bytes as that's the checksum itself.
    uint32_t crc = calculateCRC32( ((uint8_t*)&rtcData) + 4, sizeof( rtcData ) - 4 );
    if( crc == rtcData.crc32 ) {
      rtcValid = true;
    }
  }
}

void scan() {
  Serial.begin(115200);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(10);
  WiFi.forceSleepBegin();
  delay(10);
  WiFi.forceSleepWake();
  delay(100);

  Serial.println("\nscanning...");

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();

  if (n > 0) {
    for (int i = 0; i < n; ++i) {
      unsigned char *bssid = WiFi.BSSID(i);

      // Print SSID and RSSI for each network found
      Serial.print("\rnew AP: ");
      Serial.print(WiFi.SSID(i));
      Serial.print("\t(");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")\t");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" \t":"*\t");
      delay(10);
    }
  }
}

void wifi_setup() {
  int s,attempt_cnt=0, rescan_cnt=0;

  Serial.print("Connecting to: ");
  Serial.println(ssid);
  WiFi.forceSleepWake();
  delay( 1 );
  WiFi.persistent(false); // do not save wifi creds
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  while ((s=WiFi.status()) != WL_CONNECTED) {// && WiFi.localIP().toString() == "(IP unset)" && WiFi.waitForConnectResult() != WL_CONNECTED) {
    switch(s) {
      case WL_IDLE_STATUS:
        Serial.println("WL_IDLE_STATUS");
        break;
      case WL_NO_SSID_AVAIL:
        Serial.println("WL_NO_SSID_AVAIL");
        break;
      case WL_SCAN_COMPLETED:
        Serial.println("WL_SCAN_COMPLETED");
        break;
      case WL_CONNECTED:
        Serial.println("WL_CONNECTED");
        break;
      case WL_CONNECTION_LOST:
        Serial.println("WL_CONNECTION_LOST");
        break;
      case WL_DISCONNECTED:
        Serial.print(".");
        break;
      case WL_CONNECT_FAILED:
        Serial.print("WL_CONNECT_FAILED - ");
        switch(s=wifi_station_get_connect_status()) {
          case STATION_IDLE:
            Serial.println("STATION_IDLE");
            break;
          case STATION_CONNECTING:
            Serial.println("STATION_CONNECTING");
            break;
          case STATION_WRONG_PASSWORD:
            Serial.println("STATION_WRONG_PASSWORD");
            break;
          case STATION_NO_AP_FOUND:
            Serial.println("STATION_NO_AP_FOUND");
            break;
          case STATION_CONNECT_FAIL:
            Serial.println("STATION_CONNECT_FAIL");
            break;
          case STATION_GOT_IP:
            Serial.println("STATION_GOT_IP");
            break;
          default:
            Serial.print("unknown station error -");
            Serial.println(s);
            break;
        }
        s = WL_IDLE_STATUS;
        break;
      default:
        Serial.print("unknown return value - ");
        Serial.println(s);
        WiFi.printDiag(Serial);
        s = WL_IDLE_STATUS;
        break;
    }
    delay(CONNECTION_ATTEMPT_DELAY);
    if(attempt_cnt++>CONNECTION_ATTEMPT_NUM_TRIES || s==WL_IDLE_STATUS) {
      Serial.print("\nattempting connection..");
      attempt_cnt=0;
      rescan_cnt++;
      if (rescan_cnt == RESCAN_MAX_COUNT ) {
        rescan_cnt=0;
        rtcValid=false; // force a wifi restart
        scan();
      }

      if( rtcValid ) {
        // The RTC data was good, make a quick connection
        Serial.print("using stored wifi details, channel ");
        Serial.print(rtcData.channel);
        Serial.print("\n");
        WiFi.begin( ssid, password, rtcData.channel, rtcData.bssid, true );
      }
      else {
        // The RTC data was not valid, so make a regular connection
        WiFi.begin( ssid, password );
      }
      WiFi.config( ip, gateway, subnet ); // static DHCP data, should probably save this on sleep
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  sleepSave(); // save wifi stuff for quick connect on sleep resume
}
void reconnect() {
  int ret;
  int retry_count=0;
  while (!client.connected() && retry_count++<CONNECTION_ATTEMPT_NUM_TRIES) {
    Serial.print("Attempting MQTT connection...");
    String ClientId = "ESP8266";
    ClientId += String(random(0xffff), HEX);
    if (client.connect(ClientId.c_str())) {
      Serial.println("connected");
      client.publish(mqtt_topic_status, HOSTNAME" is alive!");
      ret=client.subscribe(mqtt_topic_in);
      Serial.print("subscribe returned: ");
      Serial.println(ret);
      Serial.println("MQTT connected");
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      Serial.println("Try again...");
      delay(CONNECTION_ATTEMPT_DELAY);
    }
  }
}
void sendMQTTMessage(const char* topic, const char* value) {
  int retry_count=0;
  Serial.println("sending your message");
  if (!client.connected()) {
    reconnect();
  }
  client.publish(topic, value, true);
}

void setup() {
  Serial.begin(115200);
  Serial.print("\n");
  sleepResume();

  // reduce boot power by turning off wifi radio
  WiFi.mode( WIFI_OFF );
  WiFi.forceSleepBegin();
  delay( 1 );

  // geiger setup
  radiationWatch.setup();
  radiationWatch.registerRadiationCallback(&onRadiation);
  radiationWatch.registerNoiseCallback(&onNoise);

  // turn on wifi
  wifi_setup();
  Serial.println("wifi connected at ");
  client.setServer(mqtt_server, 1883);

  sendMQTTMessage(mqtt_topic_status, HOSTNAME" booted!");
}

void onRadiation()
{
  char message[32];

  Serial.println("A wild gamma ray appeared");
  snprintf(message,32, "%f uSv/h +/- %f", radiationWatch.uSvh(), radiationWatch.uSvhError());
  Serial.println(message);

  sendMQTTMessage(mqtt_topic_out, message);
}

void onNoise()
{
  Serial.println("Argh, noise, please stop moving");
  sendMQTTMessage(mqtt_topic_status, "noise!");
}

void loop() {
  radiationWatch.loop();
}
