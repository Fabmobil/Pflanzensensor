// als erstes definieren wir die Pins der LED, des Sensors sowie den Schwellwert für den Vergleich 
const int SENSOR_PIN = A0; 
const int LED_PIN = 13; 
const int SCHWELLWERT = 500; 

// in der setup()-Funktion wird die serielle Schnittstelle aktiviert und der LED-Pin als Ausgang definiert 
void setup() { 
  Serial.begin(9600); 
  pinMode(LED_PIN, OUTPUT); 
} 

// in der loop() Funktion wird.. 
void loop() { 
  // .. die Feuchtigkeit mit Hilfe der Funktion leseFeuchtigkeit() ausgelesen: 
  int feuchtigkeit = leseFeuchtigkeit();  
  // .. mit Hilfe der Funktion zeigeFeuchtigkeitAn() angezeigt: 
  zeigeFeuchtigkeitAn(feuchtigkeit); 
  // .. 1000ms gewartet 
  delay(1000); 
} 

// die Funktion leseFeuchtigkeit() gibt das ausgelesene Signal des Bodenfeuchtesensors zurück 
int leseFeuchtigkeit() { 
  return analogRead(SENSOR_PIN); 
} 

// die Funktion zeigeFeuchtigkeitAn bekommt den Bodenfeuchtemessert und .. 
void zeigeFeuchtigkeitAn(int feuchtigkeit) { 
    // .. gibt ihn auf der seriellen Schnittstelle aus 
  Serial.print("Feuchtigkeit: "); 
  Serial.println(feuchtigkeit); 
   
    // .. vergleicht ihn mit dem Schwellwert: 
  if (feuchtigkeit > SCHWELLWERT) { 
    digitalWrite(LED_PIN, HIGH); // LED aus 
    Serial.println("Pflanze benötigt kein Wasser"); 
  } else { 
    digitalWrite(LED_PIN, LOW); // LED an 
    Serial.println("Pflanze benötigt Wasser"); 
  } 
} 
