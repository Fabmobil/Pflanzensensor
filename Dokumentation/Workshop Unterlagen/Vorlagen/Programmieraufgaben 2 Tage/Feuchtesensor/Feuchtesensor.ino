#define sensor A0 // Der Bodenfeuchtesensor steckt an Pin A0

void setup() 
{
  Serial.begin(115200); // serielle Kommunikation mit einer Geschwindigkeit von 115200 Bits pro Sekunde initialisieren
}

void loop() 
{
  int sensorWert = analogRead(sensor); // Wert des Sensors an Analogping A0 auslesen und in der Variable "sensorWert" speichern
  Serial.println(sensorWert); // Variable "sensorWert" auf der seriellen Schnittstelle auslesen
  delay(100);        // 100ms warten
}

