# ESP8266-Home-Automation
THIS CODE IS STILL IN DEVELOPMENT AND ANY BUGS AND REPORTS SHALL BE HIGHLY ENCOURAGED 
## Go here to learn more -> https://github.com/bm-a/ESP8266-Home-Automation/wiki (see Wiki for any further information.)



This repository contains the code for controlling relays and managing Wi-Fi connections using ESP8266 microcontroller. The code provides functionalities such as relay control, synchronization with NTP server, Wi-Fi management, logging, and web page interface with authentication.

## Requirements

To run the ESP8266 code, you will need the following components:

- ESP8266 microcontroller

- Relays connected to pins 1, 2, 3, and 4 (modify according to your needs)

- Manual switches connected to pins 5, 6, 7, and 8 (modify according to your needs)

- Internet connectivity for NTP synchronization

- Wi-Fi network (CONFIFURE THE SSID AND PASSWORD ACCORDINGLY)

## Functionality

1. Relay Control: The code enables the control of relays connected to pins 1, 2, 3, and 4. You can toggle the relays on or off using the web page interface.

2. Manual Switches: The manual switches connected to pins 5, 6, 7, and 8 allow you to manually control the relays.

3. NTP Time Synchronization: The code synchronizes the time with an NTP server, such as NTP India, every 5 minutes. This ensures accurate timekeeping for various functionalities. MY WIFI HAS A LOGOUT FEATURE IN CASE THERE IS NO INTERNET ACTIVITY FOR 15 MINUTES SO I DID THAT. YOU CAN CHANGE THE SYNC TIME ACCORDING TO YOUR NEEDS

4. Wi-Fi Connection Management: The code maintains a continuous Wi-Fi connection. It provides a hotspot with SSID "esp" and password "esp" when the Wi-Fi is not connected. You can configure the ssid of wifi and hotspot using this.

5. Logging: The code generates a log file that includes information on relay toggling, Wi-Fi password changes, and user logins. Logs older than 30 days are automatically deleted by editing the log file. The logs can be accessed by the admin user through a container on the web page.

6. Web Page Interface: The code provides a web page with the following functionalities:

   - Toggle Relays: Allows users to toggle the state of the relays.

   - Change Wi-Fi Credentials: Only accessible to the admin user, this feature enables changing the Wi-Fi credentials.

   - Change Hotspot Settings: The admin user can turn on the hotspot when the Wi-Fi connection is not available.

   - Login Page for "Hostel" Wi-Fi: Displays an iframe with a login page for the "Hostel" Wi-Fi network at URL: http://192.168.1.1:8090/httpclient.html. THIS WAS THE LOGIN PAGE IN MY CASE JUST COMMENT OUT THE CODE IN CASE YOU DON'T WANT THIS

   - Display Logs: The admin user can view the logs in a container on the web page.

7. Authentication: The code implements an authentication webpage with an administrator account ("admin," password: "admin") and a user account ("user," password: "user"). The user account has limited access and cannot change Wi-Fi or hotspot settings, view logs, or see usernames other than the available options in the drop-down menu. Only the administrator can access logs and change Wi-Fi/hotspot credentials.

8. OTA Updates: The ESP8266 is enabled to receive firmware updates wirelessly using the OTA functionality. A graphical interface allows users to upload the firmware bin file and trigger OTA updates wirelessly. Please follow the procedure below to generate the bin file.

9. CSS Styles: The web page is styled with a pixel-themed CSS style for an enhanced visual experience.

## OTA Updates Procedure

To generate the firmware bin file and perform OTA updates, follow these steps:

1. Make sure you have the Arduino IDE installed on your computer.

2. Open the Arduino IDE and create a new sketch.

3. Copy and paste the code you want to upload to the ESP8266.

4. Connect the ESP8266 to your computer via USB.

5. Select the appropriate board and COM port in the Arduino IDE

.

6. Click on "Sketch" > "Export compiled Binary" to generate the bin file.

7. Save the bin file to a location on your computer.

8. In the web page interface, find the "OTA Update" button and click on it.

9. Select the bin file you saved earlier and click on "Upload."

10. The ESP8266 will start receiving the OTA update wirelessly.

Note: Ensure that your ESP8266 is connected to the same network as the computer running the web page interface for OTA updates to work properly.

## License

This code is released under the [MIT License](https://opensource.org/licenses/MIT).

Please refer to the individual source files for any additional licenses or attributions.

## Acknowledgments

We acknowledge the contributions and inspiration from various open-source projects and the ESP8266 community.

If you encounter any issues or have any questions, please feel free to open an issue in this repository. We appreciate your feedback and contributions to improving this codebase. And also for any feature request.

Thank you and happy coding!
Please contact me here in case of any queries
bhavishyamadan@gmail.com



you can also consider supporting me 
https://www.paypal.me/bhavishyamadan


