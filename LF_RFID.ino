#include <Arduino.h>
#include <SparkFun_TB6612.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <MFRC522.h>

// Pin definitions for IR sensors
const int leftIRPin = 2;   // IR sensor connected to pin D2
const int rightIRPin = 4;  // IR sensor connected to pin D4

// Threshold values for IR sensors
const int threshold = 500; // Adjust this value based on sensor readings

// Motor control pins
const int PWMA = 13;
const int AIN1 = 27;
const int AIN2 = 14;
const int PWMB = 32;
const int BIN1 = 25;
const int BIN2 = 33;
const int STBY = 26;

// these constants are used to allow you to make your motor configuration 
// line up with function names like forward.  Value can be 1 or -1
const int offsetA = 1;
const int offsetB = 1;

// Initializing motors
Motor motor1 = Motor(AIN1, AIN2, PWMA, offsetA, STBY);
Motor motor2 = Motor(BIN1, BIN2, PWMB, offsetB, STBY);

// WiFi credentials
const char* ssid = "GANPATI";     // SSID (name) of the ESP32 AP
const char* password = "PAPAKOPATAHAI";    // Password of the ESP32 AP

WebServer server(80);

#define RST_PIN     5          // Configurable, see typical pin layout above
#define SS_PIN      21         // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

// Variable to store the current item the Navcart should stop at
String currentItem = "";
bool listComplete = false;
int itemIndex = 0;
String itemList[10]; // Maximum of 10 items

void setup() {
  Serial.begin(9600);
  pinMode(leftIRPin, INPUT);
  pinMode(rightIRPin, INPUT);

  // Connect to WiFi network
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");

  server.on("/", HTTP_GET, handle_OnConnect); // Serve HTML when root page is accessed
  server.on("/list", HTTP_POST, handle_list); // Handle form submission
  server.onNotFound(handle_NotFound); // Handle 404 not found

  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522 card

  Serial.println("Navcart RFID Initialized");

  server.begin();

  Serial.println("ESP32 Setup Complete");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}
// Define an array to store scanned card IDs
String scannedCardIDs[10]; // Assuming a maximum of 10 unique cards can be scanned

// Function to check if a card ID has already been scanned
bool isCardScanned(String cardID) {
  for (int i = 0; i < 10; i++) {
    if (scannedCardIDs[i] == cardID) {
      return true; // Card ID already exists in the array
    }
  }
  return false; // Card ID not found in the array
}

void loop() {
  server.handleClient();

  // IR sensor logic
  int leftIRValue = digitalRead(leftIRPin);
  int rightIRValue = digitalRead(rightIRPin);

  //Serial.print("Left IR Sensor: ");
  //Serial.print(leftIRValue);
  //Serial.print(", Right IR Sensor: ");
  //Serial.println(rightIRValue);

  if (leftIRValue == HIGH && rightIRValue == HIGH) {
    // Both sensors see black or are off the line -> stop
    motor1.brake();
    motor2.brake();
  } else if (leftIRValue == LOW && rightIRValue == LOW) {
    // Both sensors see white -> move forward
    forward();
  } else if (leftIRValue == HIGH && rightIRValue == LOW) {
    // Left sensor sees black, right sensor sees white -> turn right
    right();
  } else if (leftIRValue == LOW && rightIRValue == HIGH) {
    // Right sensor sees black, left sensor sees white -> turn left
    left();
  }

  // RFID logic
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    // Get the UID of the card
    String tagID = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      tagID.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
      tagID.concat(String(mfrc522.uid.uidByte[i], HEX));
    }

    // Check if the card ID has already been scanned
    if (!isCardScanned(tagID)) {
      // Mark the card as scanned
      for (int i = 0; i < 10; i++) {
        if (scannedCardIDs[i] == "") {
          scannedCardIDs[i] = tagID;
          break;
        }
      }
  // Handle item based on tag ID
  if (tagID == "9324ae11") { // Tag ID for Apple
    Serial.println("Apple detected");
    currentItem = "Apple";
  } else if (tagID == "835b6c11") { // Tag ID for Basketball
    Serial.println("Basketball detected");
    currentItem = "Basketball";
  } else if (tagID == "530f7b11") { // Tag ID for Crayon
    Serial.println("Crayon detected");
    currentItem = "Crayon";
  } else if (tagID == "72423f51") { // Tag ID for Dove
    Serial.println("Dove detected");
    currentItem = "Dove";

  }
  // Print the tagID
  Serial.println("Tag ID: " + tagID);

  // Check if the detected item matches any item in the list
  for (int i = 0; i < 10; i++) { // Loop through the itemList array
    if (itemList[i] == currentItem) { // If current item matches an item in the list
      // Stop the Navcart for 5 seconds
      motor1.brake();
      motor2.brake();
      Serial.println("Stopping at: " + currentItem);
      delay(5000); // Pause for 5 seconds
      currentItem = ""; // Reset currentItem

      // Move to the next item in the list
      itemIndex++;
      
      // Check if the list is completed
      if (itemIndex >= 10 || itemList[itemIndex].isEmpty()) {
        // List is completed or reached the end, reset the index
        itemIndex = 0;
        listComplete = true;
      } else {
        listComplete = false;
      }
      
      break; // Exit the loop since the item is found
    }
  }
    }
  
  // Check for separator ',' and move to the next item
  if (listComplete == false) {
    if (currentItem.isEmpty()) {
      // Get the next item from the list
      currentItem = itemList[itemIndex];
      // Move to the next item in the list
      itemIndex++;
      if (itemIndex >= 10 || itemList[itemIndex].isEmpty()) {
        // List is completed or reached the end, reset the index
        itemIndex = 0;
        listComplete = true;
       }
     }
   }
  }
}

// Function to handle root page
void handle_OnConnect() {
  server.send(200, "text/html", SendHTML());
}

// Function to handle form submission
void handle_list() {
  String receivedData = server.arg("list"); // Get the list data from the form
  Serial.println("Received list:");
  Serial.println(receivedData);  // Print received data to Serial Monitor

  // Parse the received list and store items in itemList array
  int numItems = parseItemList(receivedData);

  if (numItems == 0) {
    listComplete = true;
    currentItem = "";
    itemIndex = 0; // Reset itemIndex
  } else {
    listComplete = false;
  }

  server.send(200, "text/html", "List received successfully!"); // Send acknowledgment to the web page
}

// Function to parse the received list and store items in itemList array
int parseItemList(String list) {
  int count = 0;
  String currentItem = "";

  for (int i = 0; i < list.length(); i++) {
    if (list.charAt(i) == ',') {
      // Separator found, add currentItem to itemList array
      itemList[count++] = currentItem;
      currentItem = "";
    } else {
      currentItem += list.charAt(i);
    }
  }

  // Add the last item to itemList array
  if (!currentItem.isEmpty()) {
    itemList[count++] = currentItem;
  }

  return count;
}

// Function to handle 404 not found
void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

// Function to generate and return HTML code

  String SendHTML() {
  // HTML code for your webpage
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head>\n";
  ptr +="<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>ESP32 Web Server</title>\n";
  ptr +="<style>\n";
ptr +="*{\n";
ptr +="            margin: 0;\n";
ptr +="            padding: 0;\n";
ptr +="        }\n";
ptr +="        .main{\n";
ptr +="            width: 100%;\n";
ptr +="            background: linear-gradient(to top, rgba(0,0,0,0.5)50%,rgba(0,0,0,0.5)50%), url(1.png);\n";
ptr +="            background-position: center;\n";
ptr +="            background-size: cover;\n";
ptr +="            height: 100vh;\n";
ptr +="        }\n";
ptr +="        .navbar{\n";
ptr +="            width: 1200px;\n";
ptr +="            height: 75px;\n";
ptr +="            margin: auto;\n";
ptr +="        }\n";
ptr +="        .icon{\n";
ptr +="            width: 200px;\n";
ptr +="            float: left;\n";
ptr +="            height: 70px;\n";
ptr +="        }\n";
ptr +="        .logo{\n";
ptr +="            color: #ff7200;\n";
ptr +="            font-size: 35px;\n";
ptr +="            font-family: Arial;\n";
ptr +="            padding-left: 20px;\n";
ptr +="            float: left;\n";
ptr +="            padding-top: 10px;\n";
ptr +="            margin-top: 5px\n";
ptr +="        }\n";
ptr +="        .menu{\n";
ptr +="            width: 400px;\n";
ptr +="            float: left;\n";
ptr +="            height: 70px;\n";
ptr +="        }\n";
ptr +="        ul{\n";
ptr +="            float: left;\n";
ptr +="            display: flex;\n";
ptr +="            justify-content: center;\n";
ptr +="            align-items: center;\n";
ptr +="        }\n";
ptr +="        ul li{\n";
ptr +="            list-style: none;\n";
ptr +="            margin-left: 62px;\n";
ptr +="            margin-top: 27px;\n";
ptr +="            font-size: 14px;\n";
ptr +="        }\n";
ptr +="        ul li a{\n";
ptr +="            text-decoration: none;\n";
ptr +="            color: #fff;\n";
ptr +="            font-family: Arial;\n";
ptr +="            font-weight: bold;\n";
ptr +="            transition: 0.4s ease-in-out;\n";
ptr +="        }\n";
ptr +="        ul li a:hover{\n";
ptr +="            color: #ff7200;\n";
ptr +="        }\n";
ptr +="        .btn{\n";
ptr +="            width: 100px;\n";
ptr +="            height: 40px;\n";
ptr +="            background: #ff7200;\n";
ptr +="            border: 2px solid #ff7200;\n";
ptr +="            margin-top: 13px;\n";
ptr +="            color: #fff;\n";
ptr +="            font-size: 15px;\n";
ptr +="            border-bottom-right-radius: 5px;\n";
ptr +="            border-bottom-right-radius: 5px;\n";
ptr +="            transition: 0.2s ease;\n";
ptr +="            cursor: pointer;\n";
ptr +="        }\n";
ptr +="        .btn:hover{\n";
ptr +="            color: #000;\n";
ptr +="        }\n";
ptr +="        .btn:focus{\n";
ptr +="            outline: none;\n";
ptr +="        }\n";
ptr +="        .srch:focus{\n";
ptr +="            outline: none;\n";
ptr +="        }\n";
ptr +="        .content{\n";
ptr +="            width: 1200px;\n";
ptr +="            height: auto;\n";
ptr +="            margin: auto;\n";
ptr +="            color: #fff;\n";
ptr +="            position: relative;\n";
ptr +="        }\n";
ptr +="        .content .par{\n";
ptr +="            padding-left: 20px;\n";
ptr +="            padding-bottom: 25px;\n";
ptr +="            font-family: Arial;\n";
ptr +="            letter-spacing: 1.2px;\n";
ptr +="            line-height: 30px;\n";
ptr +="        }\n";
ptr +="        .content h1{\n";
ptr +="            font-family: 'Times New Roman';\n";
ptr +="            font-size: 50px;\n";
ptr +="            padding-left: 20px;\n";
ptr +="            margin-top: 9%;\n";
ptr +="            letter-spacing: 2px;\n";
ptr +="        }\n";
ptr +="        .content .cn{\n";
ptr +="            width: 160px;\n";
ptr +="            height: 40px;\n";
ptr +="            background: #ff7200;\n";
ptr +="            border: none;\n";
ptr +="            margin-bottom: 10px;\n";
ptr +="            margin-left: 20px;\n";
ptr +="            font-size: 18px;\n";
ptr +="            border-radius: 10px;\n";
ptr +="            cursor: pointer;\n";
ptr +="            transition: .4s ease;\n";
ptr +="        }\n";
ptr +="        .content .cn a{\n";
ptr +="            text-decoration: none;\n";
ptr +="            color: #000;\n";
ptr +="            transition: .3s ease;\n";
ptr +="        }\n";
ptr +="        .cn:hover{\n";
ptr +="            background-color: #fff;\n";
ptr +="        }\n";
ptr +="        .content span{\n";
ptr +="            color: #ff7200;\n";
ptr +="            font-size: 65px\n";
ptr +="        }\n";
ptr +="        .form{\n";
ptr +="            width: 250px;\n";
ptr +="            height: 380px;\n";
ptr +="            background: linear-gradient(to top, rgba(0,0,0,0.8)50%,rgba(0,0,0,0.8)50%);\n";
ptr +="            position: absolute;\n";
ptr +="            top: -20px;\n";
ptr +="            left: 870px;\n";
ptr +="            transform: translate(0%,-5%);\n";
ptr +="            border-radius: 10px;\n";
ptr +="            padding: 25px;\n";
ptr +="        }\n";
ptr +="        .form h2{\n";
ptr +="            width: 220px;\n";
ptr +="            font-family: sans-serif;\n";
ptr +="            text-align: center;\n";
ptr +="            color: #ff7200;\n";
ptr +="            font-size: 22px;\n";
ptr +="            background-color: #fff;\n";
ptr +="            border-radius: 10px;\n";
ptr +="            margin: 2px;\n";
ptr +="            padding: 8px;\n";
ptr +="        }\n";
ptr +="        .form input{\n";
ptr +="            width: 240px;\n";
ptr +="            height: 35px;\n";
ptr +="            background: transparent;\n";
ptr +="            border-bottom: 1px solid #ff7200;\n";
ptr +="            border-top: none;\n";
ptr +="            border-right: none;\n";
ptr +="            border-left: none;\n";
ptr +="            color: #fff;\n";
ptr +="            font-size: 15px;\n";
ptr +="            letter-spacing: 1px;\n";
ptr +="            margin-top: 30px;\n";
ptr +="            font-family: sans-serif;\n";
ptr +="        }\n";
ptr +="        .form input:focus{\n";
ptr +="            outline: none;\n";
ptr +="        }\n";
ptr +="        ::placeholder{\n";
ptr +="            color: #fff;\n";
ptr +="            font-family: Arial;\n";
ptr +="        }\n";
ptr +="        .btnn{\n";
ptr +="            width: 240px;\n";
ptr +="            height: 40px;\n";
ptr +="            background: #ff7200;\n";
ptr +="            border: none;\n";
ptr +="            margin-top: 30px;\n";
ptr +="            font-size: 18px;\n";
ptr +="            border-radius: 10px;\n";
ptr +="            cursor: pointer;\n";
ptr +="            color: #fff;\n";
ptr +="            transition: 0.4s ease;\n";
ptr +="        }\n";
ptr +="        .btnn:hover{\n";
ptr +="            background: #fff;\n";
ptr +="            color: #ff7200;\n";
ptr +="        }\n";
ptr +="        .btnn a{\n";
ptr +="            text-decoration: none;\n";
ptr +="            color: #000;\n";
ptr +="            font-weight: bold;\n";
ptr +="        }\n";
ptr +="        .form .link{\n";
ptr +="            font-family: Arial, Helvetica, sans-serif;\n";
ptr +="            font-size: 17px;\n";
ptr +="            padding-top: 20px;\n";
ptr +="            text-align: center;\n";
ptr +="        }\n";
ptr +="        .form .link a{\n";
ptr +="            text-decoration: none;\n";
ptr +="            color: #ff7200;\n";
ptr +="        }\n";
ptr +="        .liw{\n";
ptr +="            padding-top: 15px;\n";
ptr +="            padding-bottom: 10px;\n";
ptr +="            text-align: center;\n";
ptr +="        }\n";
ptr +="    </style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr += "<div id=\"acknowledgment\"></div>\n";
  ptr +="   <div class=\"main\">\n";
ptr +="        <div class=\"navbar\">\n";
ptr +="            <div class=\"icon\">\n";
ptr +="                <h2 class=\"logo\">NAVCart</h2>\n";
ptr +="            </div>\n";
ptr +="            <div class=\"menu\">\n";
ptr +="                <ul>\n";
ptr +="                    <li><a href=\"#\">Home</a></li>\n";
ptr +="                    <li>\n";
ptr +="                        <div class=\"dropdown\" id=\"itemsDropdown\">\n";
ptr +="                            <button class=\"dropbtn\" onclick=\"toggleItems()\">ITEMS AVAILABLE</button>\n";
ptr +="                            <div class=\"dropdown-content\" id=\"itemsList\" style=\"display: none;\">\n";
ptr +="                                <a href=\"#\">Apple</a><br>\n";
ptr +="                                <a href=\"#\">Basketball</a><br>\n";
ptr +="                                <a href=\"#\">Crayon</a><br>\n";
ptr +="                                <a href=\"#\">Dove</a><br>\n";
ptr +="                            </div>\n";
ptr +="                        </div>\n";
ptr +="                    </li>\n";
ptr +="                </ul>\n";
ptr +="            </div>\n";
ptr +="        </div>\n";
ptr +="        <div class=\"content\">\n";
ptr +="            <h1>WELCOME TO<br><span>NAVCart</span></h1>\n";
ptr +="            <p class=\"par\">\"Unleash your shopping spree at our digital haven,\n";
ptr +="                where convenience meets style.<br> Dive into a world of curated collections, <br>\n";
ptr +="                unbeatable deals, and hassle-free transactions.<br> \n";
ptr +="                Experience the joy of effortless shopping from home with swift delivery. <br>\n";
ptr +="                Join us and redefine online shopping as a journey of sheer delight.\"</p>\n";
ptr +="            <div class=\"container\">\n";
ptr +="                <form action=\"/list\" method=\"post\" id=\"shoppingForm\">\n";
ptr +="                    <div class=\"form\">\n";
ptr +="                        <h2>FOR YOU:</h2>\n";
ptr +="                        <br>\n";
ptr +="                        <input type=\"text\" id=\"inputText\" name=\"list\" placeholder=\"Enter your choice\">\n";
ptr +="                        <button type=\"button\" class=\"btnn\" onclick=\"submitForm()\">SUBMIT</button>\n";
ptr +="                        <button type=\"button\" class=\"btnn\" onclick=\"sendToESP()\">SEND</button>\n";
ptr +="                    </div>\n";
ptr +="                </form>\n";
ptr +="            </div>\n";
ptr +="        </div>\n";
ptr +="    </div>\n";
  ptr +="<script>\n";
ptr +="       function toggleItems() {\n";
ptr +="            var itemsList = document.getElementById(\"itemsList\");\n";
ptr +="            if (itemsList.style.display === \"none\") {\n";
ptr +="                itemsList.style.display = \"block\";\n";
ptr +="            } else {\n";
ptr +="                itemsList.style.display = \"none\";\n";
ptr +="            }\n";
ptr +="        }\n";
ptr +="        \n";
ptr +="        function submitForm() {\n";
ptr +="            var inputText = document.getElementById(\"inputText\").value;\n";
ptr +="            var sortedInput = sortAlphabetically(inputText);\n";
ptr +="            document.getElementById(\"inputText\").value = sortedInput;\n";
ptr +="        }\n";
ptr +="        \n";
ptr +="        function sortAlphabetically(input) {\n";
ptr +="            return input.split(',').map(function(item) {\n";
ptr +="                return item.trim().charAt(0).toUpperCase() + item.trim().slice(1);\n";
ptr +="            }).sort().join(',');\n";
ptr +="        }\n";
ptr +="        \n";
  ptr +="function sendToESP() {\n";
  ptr +="    var list = document.getElementById(\"inputText\").value;\n";
  ptr +="    var xhttp = new XMLHttpRequest();\n";
  ptr +="    xhttp.onreadystatechange = function() {\n";
  ptr +="        if (this.readyState == 4 && this.status == 200) {\n";
  ptr +="            console.log(\"Data sent to ESP32:\", list);\n";
  ptr +="        }\n";
  ptr +="    };\n";
  ptr +="    xhttp.open(\"POST\", \"/list\", true);\n";
  ptr +="    xhttp.setRequestHeader(\"Content-type\", \"application/x-www-form-urlencoded\");\n";
  ptr +="    xhttp.send(\"list=\" + list);\n";
  ptr +="}\n";
  ptr +="</script>\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
        
  return ptr;
}



void forward() {
  motor1.drive(50); // Motor A full speed forward
  motor2.drive(50); // Motor B full speed forward
}

void left() {
  motor1.drive(-100); // Motor A full speed backward (turn left)
  motor2.drive(50);  // Motor B full speed forward
}

void right() {
  motor1.drive(50);  // Motor A full speed forward
  motor2.drive(-100); // Motor B full speed backward (turn right)
}