#define LED D0 // Die interne LED ist beim ESP8266 auf Pin D0
#define sensor A0 // Der Bodenfeuchtesensor steckt an Pin A0
int schwellwert = 500; // der Schwellwert mit dem verglichen werden soll

void setup() 
{
  Serial.begin(115200); // serielle Kommunikation mit einer Geschwindigkeit von 115200 Bits pro Sekunde initialisieren
}

void loop() 
{
  int sensorwert = analogRead(sensor); // Wert des Sensors an Analogping A0 auslesen und in der Variable "sensorWert" speichern
  Serial.println(sensorwert); // Variable "sensorwert" auf der seriellen Schnittstelle ausgeben
  delay(100);        // 100ms warten
}

