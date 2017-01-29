
#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <string>
#include <ACROBOTIC_SSD1306.h>

#include "warcollar.h"

#define SCAN_PERIOD 5000
long lastScanMillis;

using std::string;

// function run once at boot up time
void ICACHE_FLASH_ATTR setup()
{
  Serial.begin(115200);   // initialize the serial console port
  Wire.begin(2, 0);       // initialize the i2c library
  oled.init();            // initialize the oled display library

  delay(200);             // short delay to let init settle down

  WiFi.mode(WIFI_STA);    // Set WiFi to station mode

  oled.clearDisplay();    // remove random junk on the display

  oled.setFont(font6x8);  // set to our custom 6x8 font with AP icons

  // Draw the warcollar logo and the version information
  oled.drawBitmap(wclogo, 1024);
  oled.setTextXY(7, 0);
  oled.putString("            v2.0.0.a");
  //             "12345678901234567890"  -  max string with 6x8 font

  // send a status message to top line of the display
  oled.setTextXY(0, 0);
  oled.putString("[--=Initializing=--]");
  delay(500);
}

// do not use the ICACHE_FLASH_ATTR on the following functions since the code
// needs to be in memory as it is called very frequently.


// function to insert a new_node in a reverse sorted list.
void insertNodeR(struct node** head_ref, struct node* new_node)
{
    struct node* current;
    // Special case if it is a new list or the new node
    // needs to be inserted at the begining
    if (*head_ref == NULL || (*head_ref)->RSSI < new_node->RSSI)
    {
        new_node->next = *head_ref;
        *head_ref = new_node;
    }
    else
    {
        current = *head_ref;
        while (current->next!=NULL &&
               current->next->RSSI > new_node->RSSI)
        {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
    }
}

// create a new node
struct node *newNode(int32_t new_rssi, uint8_t new_apindex)
{
    /* allocate memory for the node */
    struct node* new_node = (struct node*) malloc(sizeof(struct node));

    // add the current data to the new node
    new_node->RSSI  = new_rssi;
    new_node->apindex = new_apindex;
    new_node->next =  NULL;

    return new_node;
}

// function to print out and free memory of the whole list
void printList(struct node** head_ref)
{
  struct node* current = *head_ref;
  struct node* next_node;
  int loopcount = 0;
  std::string enc, ap_txt;

  // loop through the sorted list
  while (current != NULL)
  {
    uint8_t ap = current->apindex;
    next_node = current->next;

    // set the encryption type character of the current ap for our custom font
    switch (WiFi.encryptionType(ap)) {
      case ENC_TYPE_WEP:
        // WEP
        enc = "{";
        ap_txt = " WEP";
        break;
      case ENC_TYPE_TKIP:
        // WPA
        enc = "|";
        ap_txt = " WPA";
        break;
      case ENC_TYPE_CCMP:
        // WPA2
        enc = "}";
        ap_txt = "WPA2";
        break;
      case ENC_TYPE_NONE:
        enc = " ";
        ap_txt = "    ";
        break;
      case ENC_TYPE_AUTO:
        enc = "~";
        ap_txt = "AUTO";
        break;
    } // end of case loop

    // display only has 8 lines and the first is the info header
    if ( loopcount < 7 )
    {
      char ap_str[21];
      unsigned int nchr = os_sprintf(ap_str, "%d %-2d %s %-12s",WiFi.RSSI(ap), WiFi.channel(ap), enc.c_str(), WiFi.SSID(ap).substring(0,11).c_str());
      oled.setTextXY(loopcount+1, 0);
      oled.putString(ap_str);
    }

    // print all the data to the serial console
    Serial.printf("%d %-2d %s %s\n",WiFi.RSSI(ap), WiFi.channel(ap), ap_txt.c_str(), WiFi.SSID(ap).c_str());

    // recover the memory allocated for the current record
    free(current);

    // move onto the next record in the list
    current = next_node;
    loopcount += 1;
  }

  // if there were not enough Aps to fill the display
  while (loopcount < 7)
  {
    oled.setTextXY(loopcount+1, 0);
    oled.putString("                    ");
    //             "12345678901234567890"  -  max string with 6x8 font
    loopcount += 1;
  }
}

// do not put any delays in this loop, it might trigger the WDT
void loop() {
  // record the entry time into the loop
  long currentMillis = millis();

  // trigger Wi-Fi network scan if scan period exceeded
  if (currentMillis - lastScanMillis > SCAN_PERIOD)
  {
    // start async scanning and include 'hidden' networks
    // this returns immediately and the task is handled in the background
    WiFi.scanNetworks(true, true);
    Serial.print("\nScan starting ... ");
    oled.setTextXY(0, 0);
    oled.putString("[----=Scanning=----]");
    //             "12345678901234567890"  -  max string with 6x8 font

    //record the time the scan was started
    lastScanMillis = currentMillis;
  }

  // scan is asyncronous, so check if it is has completed yet
  int aps = WiFi.scanComplete();

  // networks were found if the number of aps is greater than 0
  if(aps >= 0)
  {
    // tell the outside world how many aps were found
    Serial.printf("Found %d APs\n", aps);
    char t_str[21];
    unsigned int nchr = os_sprintf(t_str, "[--=Found %-2d AP's=-]", aps);
    //                                    "12345678901234567890"  -  max string with 6x8 font
    oled.setTextXY(0, 0);
    oled.putString(t_str);

    // create an empty list
    struct node* head = NULL;

    // loop through all the found access points and sort the IDs into a list
    for (int i = 0; i < aps; i++)
    {
      struct node *new_node = newNode(WiFi.RSSI(i), i);
      insertNodeR(&head, new_node);
    }

    // do the display function and release the allocated memory
    printList(&head);

    // throw out old results so we don't get stale info next time
    WiFi.scanDelete();
  }
}
