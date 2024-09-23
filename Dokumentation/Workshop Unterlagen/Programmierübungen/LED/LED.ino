#define LED D0 // Die interne LED ist beim ESP8266 auf Pin D0

void setup() 
{
 pinMode(LED, OUTPUT); // LED wird als Ausgang definiert
}

void loop() 
{
 digitalWrite (LED,LOW); // LED anschalten
}

