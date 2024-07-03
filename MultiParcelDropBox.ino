#include <SD.h>
#include <SPI.h>
// MISO = D50
// MOSI = D51
// SCK = D52
// CS = D4
#include <LiquidCrystal_I2C.h>
//SDA = D20
//SCL = D21
//VCC = 5V arduino
//GND = GND arduino
#include <Keypad.h>

const int NUM_COMPARTMENTS = 6;

const int relayPins[NUM_COMPARTMENTS] = {22, 23, 24, 25, 26, 27}; 
const int greenLEDPins[NUM_COMPARTMENTS] = {28, 29, 30, 31, 32, 33}; 
const int redLEDPins[NUM_COMPARTMENTS] = {34, 35, 36, 37, 38, 39}; 
const int resetButton[] = {40, 41, 42, 43, 44, 45}; // Reset button pins

const byte ROWS = 4; // Four rows
const byte COLS = 4; // Four columns
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {6, 5, 3, 2}; // Connect to the row pinouts of the keypad
byte colPins[COLS] = {10, 9, 8, 7}; // Connect to the column pinouts of the keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
LiquidCrystal_I2C lcd(0x27, 20, 4);

int selectedCompartment = 0;
String password = "";

void setup() {
  Serial.begin(9600);
  Serial.print("Initializing SD card...");
  if (!SD.begin(53)) {
    Serial.println("Initialization failed!");
    while (1);
  }
  Serial.println("Initialization done.");

  for (int i = 0; i < NUM_COMPARTMENTS; i++) {
    pinMode(relayPins[i], OUTPUT);
    pinMode(greenLEDPins[i], OUTPUT);
    pinMode(redLEDPins[i], OUTPUT);
    pinMode(resetButton[i], INPUT_PULLUP);
    digitalWrite(relayPins[i], HIGH);
  }

  lcd.init(); // Initialize the LCD
  lcd.backlight(); // Turn on the LCD backlight
  lcd.setCursor(4, 0);
  lcd.print("Multi Parcel");
  lcd.setCursor(6, 1);
  lcd.print("Drop Box");
  //delay(1000);
}

void loop() {
  // Check for reset button press
  for (int i = 0; i < 6; i++) {
    if (digitalRead(resetButton[i]) == LOW) { // Button press detected
      resetCompartment(i + 1); // Reset compartment corresponding to index i
      break; // Exit loop after processing one button press
    }
  }
  
  String compartmentsString = readAvailableCompartments();
  int compartments[NUM_COMPARTMENTS];
  
  Serial.println(compartmentsString);
  int index = 0;
  char* token = strtok(const_cast<char*>(compartmentsString.c_str()), ",");

  // Parse the string into an integer array
  while (token != NULL) {
    compartments[index] = atoi(token); // Convert token to integer and store in array
    index++;
    token = strtok(NULL, ",");
  }

  // Reset LEDs
  for (int i = 0; i < NUM_COMPARTMENTS; i++) {
    digitalWrite(greenLEDPins[i], LOW);
    digitalWrite(redLEDPins[i], LOW);
  }

  // Update LEDs based on availability
  for (int i = 0; i < NUM_COMPARTMENTS; i++) {
    bool isAvailable = false;
    for (int x = 0; x < index; x++) {
      if (i + 1 == compartments[x]) {
        isAvailable = true;
        break;
      }
    }
    if (isAvailable) {
      digitalWrite(greenLEDPins[i], HIGH);
    } else {
      digitalWrite(redLEDPins[i], HIGH);
    }
  }

  char key = keypad.getKey();
  if (key == '*') {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Select Compartment:");
    lcd.setCursor(0, 1);
    lcd.print("1  2  3  4  5  6");

    // Wait for valid compartment selection (digits '1' to '6')
    selectedCompartment = 0;
    while (!(selectedCompartment >= 1 && selectedCompartment <= NUM_COMPARTMENTS)) {
      char selectedChar = keypad.getKey();
      if (selectedChar == 'D'){
        showMainScreen();
        break;
      }
      if (selectedChar >= '1' && selectedChar <= '6') {
        selectedCompartment = selectedChar - '0';
        break;
      }
    }

    if (selectedCompartment == 0){
      return;
    }
    // Check if selected compartment is available (password set)
    bool isAvailable = false;
    for (int i = 0; i < index; i++) {
      if (compartments[i] == selectedCompartment) {
        isAvailable = true;
        break;
      }
    }

    // Check if a password is already set for the selected compartment
    String fileName = "compart" + String(selectedCompartment) + ".txt";
    bool passwordSet = SD.exists(fileName);

    if (passwordSet) {
      // Prompt for password entry to open compartment
      handlePasswordEntry();
    } else {
      // Prompt for setting a new password
      handlePasswordSetup();
    }
  }
}

String readAvailableCompartments() {
  String result = "";
  File compartmentsFile = SD.open("compart.txt");
  if (compartmentsFile) {
    while (compartmentsFile.available()) {
      String compartmentsData = compartmentsFile.readStringUntil('\n');
      if (compartmentsData.length() > 0) {
        result = compartmentsData;
        break;
      }
    }
    compartmentsFile.close();
  } else {
    Serial.println("Error opening compart.txt");
  }
  return result;
}

void activateCompartment(int compartment) {
  if (compartment >= 1 && compartment <= NUM_COMPARTMENTS) {
    int relayIndex = compartment - 1;
    digitalWrite(relayPins[relayIndex], LOW); // Activate the relay
    
    // Display message on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Close the door");
    lcd.setCursor(0, 1);
    lcd.print("then press OK to");
    lcd.setCursor(0, 2);
    lcd.print("lock the door");

    // Wait for user to press "*" key to deactivate the relay
    bool deactivate = false;
    unsigned long startTime = millis();
    unsigned long timeoutDuration = 60000; // 60 seconds timeout
    
    while (!deactivate) {
      char key = keypad.getKey();
      if (key == '*') {
        deactivate = true;
      }
      // Check if timeout has occurred
      if (millis() - startTime > timeoutDuration) {
        deactivate = true; // Timeout reached, deactivate relay
      }
    }
    
    digitalWrite(relayPins[relayIndex], HIGH); // Deactivate the relay
  }
}

bool checkPassword(int compartment, String enteredPassword) {
  String fileName = "compart" + String(compartment) + ".txt";
  File passwordFile = SD.open(fileName);

  if (passwordFile) {
    String savedPassword = passwordFile.readStringUntil('\n');
    savedPassword.trim();
    passwordFile.close();
    Serial.println(enteredPassword);
    Serial.println(savedPassword);
    Serial.println(savedPassword.length());
    Serial.println(enteredPassword.length());
    return enteredPassword == savedPassword;
  } else {
    Serial.println("Error opening " + fileName);
  }

  return false;
}

void savePassword(int compartment, String password) {
  String filename = "compart" + String(compartment) + ".txt";
  
  if (SD.exists(filename)) {
    SD.remove(filename);
  }
  
  File passwordFile = SD.open(filename, FILE_WRITE);
  
  if (passwordFile) {
    passwordFile.println(password);
    passwordFile.close();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Password saved");
    lcd.setCursor(0, 1);
    lcd.print("to compartment ");
    lcd.print(compartment);
    delay(2000); // Display confirmation for 2 seconds
    
    // Update available compartments
    updateAvailableCompartments(compartment);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error saving");
    lcd.setCursor(0, 1);
    lcd.print("password!");
    delay(2000); // Display error message for 2 seconds
  }
}

void handlePasswordEntry() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Password:");

  password = "";
  lcd.setCursor(0, 1);
  while (true) {
    char key = keypad.getKey();

    if (key == 'D'){
      showMainScreen();
      break;
    }

    if (key != NO_KEY) {
      if (key == '#') {
        if (password.length() > 0) {
          password.remove(password.length() - 1);
          lcd.setCursor(password.length(), 1);
          lcd.print(" "); // Clear the deleted character on LCD
          lcd.setCursor(password.length(), 1);
        }
      } else if (key == '*') {
        if (checkPassword(selectedCompartment, password)) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Opening compartment ");
          lcd.print(selectedCompartment);
          activateCompartment(selectedCompartment);
          delay(2000); // Display confirmation for 2 seconds
          showMainScreen();
          break;
        } else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Incorrect password");
          lcd.setCursor(0, 1);
          lcd.print("try again");
          delay(2000);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Enter Password:");
          lcd.setCursor(0, 1);
          password = "";
        }
      } else if (password.length() < 8) {
        password += key;
        lcd.print('*'); // Display asterisk for each character entered
      }
    }
  }
}

void handlePasswordSetup() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set password to");
  lcd.setCursor(0, 1);
  lcd.print("compartment ");
  lcd.print(selectedCompartment);
  lcd.setCursor(0, 2);
  lcd.print("min: 4, max: 8");

  password = "";
  lcd.setCursor(0, 3);
  lcd.print("PASS: ");
  while (true) {
    char key = keypad.getKey();

    if (key == 'D'){
      showMainScreen();
      break;
    }

    if (key != NO_KEY) {
      if (key == '#') {
        if (password.length() > 0) {
          password.remove(password.length() - 1);
          lcd.setCursor(6 + password.length(), 3);
          lcd.print(" "); // Clear the deleted character on LCD
          lcd.setCursor(6 + password.length(), 3);
        }
      } else if (key == '*') {
        if (password.length() >= 4) {
          savePassword(selectedCompartment, password);
          activateCompartment(selectedCompartment);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Password set:");
          lcd.setCursor(0, 1);
          lcd.print(password);
          delay(10000); // Display confirmation for 2 seconds
          showMainScreen();
          break;
        } else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Password must be");
          lcd.setCursor(0, 1);
          lcd.print("at least 4 chars");
          delay(2000);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Set password to");
          lcd.setCursor(0, 1);
          lcd.print("compartment ");
          lcd.print(selectedCompartment);
          lcd.setCursor(0, 2);
          lcd.print("min: 4, max: 8");
          lcd.setCursor(0, 3);
          lcd.print("PASS: ");
          lcd.setCursor(6, 3);
        }
      } else if (password.length() < 8) {
        password += key;
        lcd.print('*'); // Display asterisk for each character entered
      }
    }
  }
}

void showMainScreen() {
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("Multi Parcel");
  lcd.setCursor(6, 1);
  lcd.print("Drop Box");
}

void updateAvailableCompartments(int usedCompartment) {
  String availableCompartments = readAvailableCompartments();
  String updatedCompartments = "";
  int start = 0;
  int end = availableCompartments.indexOf(',');

  while (end != -1) {
    int compartment = availableCompartments.substring(start, end).toInt();
    if (compartment != usedCompartment) {
      updatedCompartments += String(compartment) + ",";
    }
    start = end + 1;
    end = availableCompartments.indexOf(',', start);
  }

  int lastCompartment = availableCompartments.substring(start).toInt();
  if (lastCompartment != usedCompartment) {
    updatedCompartments += String(lastCompartment);
  } else if (updatedCompartments.endsWith(",")) {
    updatedCompartments = updatedCompartments.substring(0, updatedCompartments.length() - 1);
  }

  // Write updated compartments to file
  File compartmentsFile = SD.open("compart.txt", FILE_WRITE | O_TRUNC);
  if (compartmentsFile) {
    compartmentsFile.println(updatedCompartments);
    compartmentsFile.close();
  } else {
    Serial.println("Error opening compart.txt for writing");
  }
}

void resetCompartment(int compartment) {
  // Check if password is set for the compartment
  String fileName = "compart" + String(compartment) + ".txt";
  bool passwordSet = SD.exists(fileName);

  // Display message and exit if password is not set
  if (!passwordSet) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Compartment ");
    lcd.print(compartment);
    lcd.setCursor(0, 1);
    lcd.print("has no saved");
    lcd.setCursor(0,2);
    lcd.print("password yet");
    delay(2000);
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("Multi Parcel");
    lcd.setCursor(6, 1);
    lcd.print("Drop Box");

    return;
  }

  // Prompt for password entry to reset compartment
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Password:");
  password = "";
  lcd.setCursor(0, 1);
  while (true) {
    char key = keypad.getKey();

    if (key == 'D'){
      showMainScreen();
      break;
    }

    if (key != NO_KEY) {
      if (key == '#') {
        if (password.length() > 0) {
          password.remove(password.length() - 1);
          lcd.setCursor(password.length(), 1);
          lcd.print(" "); // Clear the deleted character on LCD
          lcd.setCursor(password.length(), 1);
        }
      } else if (key == '*') {
        if (checkPassword(compartment, password)) {
          // Password correct, proceed with reset
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Resetting");
          lcd.setCursor(0,1);
          lcd.print("Compartment");
          lcd.print(compartment);
          delay(2000);

          // Remove password file
          SD.remove(fileName);
          Serial.println(fileName + " removed.");

          // Update available compartments list
          String availableCompartments = readAvailableCompartments();
          String updatedCompartments = "";

          // Remove compartment from available compartments if exists
          String compartmentStr = String(compartment);
          int pos = availableCompartments.indexOf(compartmentStr);
          if (pos != -1) {
            updatedCompartments = availableCompartments.substring(0, pos);
            if (pos + compartmentStr.length() < availableCompartments.length()) {
              updatedCompartments += availableCompartments.substring(pos + compartmentStr.length() + 1);
            }
          } else {
            updatedCompartments = availableCompartments; // No change needed
          }

          // Append reset compartment to updated list
          if (updatedCompartments != "") {
              updatedCompartments += ",";
          }
          updatedCompartments += compartmentStr;

          // Write updated available compartments list to file
          File compartmentsFile = SD.open("compart.txt", FILE_WRITE | O_TRUNC);
          if (compartmentsFile) {
            Serial.println(updatedCompartments);
            compartmentsFile.println(updatedCompartments);
            compartmentsFile.close();
            Serial.println("Updated available compartments: " + updatedCompartments);
          } else {
            Serial.println("Error opening compart.txt for writing.");
          }

          // Display finished message on LCD
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Finished resetting");
          lcd.setCursor(0, 1);
          lcd.print("compartment ");
          lcd.print(compartment);
          delay(2000); // Display message for 2 seconds
          break; // Exit password entry loop
        } else {
          // Incorrect password entered
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Incorrect password");
          lcd.setCursor(0, 1);
          lcd.print("try again");
          delay(2000);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Enter Password:");
          lcd.setCursor(0, 1);
          password = "";
        }
      } else if (password.length() < 8) {
        // Append character to password if length is less than 8
        password += key;
        lcd.print('*'); // Display asterisk for each character entered
      }
    }
  }

  // Return to main screen after resetting compartment
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("Multi Parcel");
  lcd.setCursor(6, 1);
  lcd.print("Drop Box");
  delay(3000); // Display main screen for 3 seconds
}

