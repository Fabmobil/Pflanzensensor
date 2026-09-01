/**
 * @file mail_sender.cpp
 * @brief Umsetzung des Mailversands (siehe mail_sender.h)
 */

#include "mail/mail_sender.h"

#include "mail/mail_vorlagen.h"
#include "utils/mail_template.h"

#include <WiFiClientSecure.h>

#include <memory>
#include <new>

#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor.h"
#include "utils/helper.h"
#include "utils/mail_message.h"
#include "utils/memory_manager.h"
#include "utils/smtp_session.h"
#include "web/core/web_manager.h"

extern std::unique_ptr<SensorManager> sensorManager;

namespace {

/// Wie lange auf eine Antwortzeile gewartet wird.
constexpr uint32_t ANTWORT_FRIST_MS = 15000;
/// Obergrenze für eine Antwortzeile - schützt vor einem Server, der endlos
/// Zeichen ohne Zeilenende schickt.
constexpr size_t ZEILE_MAX = 256;

/// Alle 30 Sekunden prüfen, ob etwas ansteht. Häufiger wäre sinnlos: die
/// kürzeste Sperre ist eine Stunde.
constexpr uint32_t PRUEFINTERVALL_MS = 30000;

/// Speicherstand melden, wenn "Debug RAM" eingeschaltet ist.
///
/// Der Mailversand ist der speicherhungrigste Vorgang auf diesem Gerät - hier
/// zu sehen, wieviel vor, während und nach dem Handshake übrig war, ist der
/// einzige Weg, eine Fehlermeldung wie "Zu wenig Speicher" einzuordnen.
void meldeSpeicher(const __FlashStringHelper* wann) {
  if (!ConfigMgr.isDebugRAM()) {
    return;
  }
  LOG_DEBUG(F("Mail"), String(F("Speicher ")) + String(wann) + F(": ") + String(ESP.getFreeHeap()) +
                           F(" B frei, groesster Block ") + String(ESP.getMaxFreeBlockSize()) +
                           F(" B, Fragmentierung ") + String(ESP.getHeapFragmentation()) + F(" %"));
}

Mail::Level levelVon(const String& status) { return Mail::levelVonText(status.c_str()); }

/// Eine Zeile bis CRLF lesen. Gibt false bei Zeitüberschreitung zurück.
bool leseZeile(WiFiClient& client, String& zeile) {
  zeile = "";
  const uint32_t frist = millis() + ANTWORT_FRIST_MS;
  while (millis() < frist) {
    while (client.available()) {
      const char c = static_cast<char>(client.read());
      if (c == '\n') {
        while (zeile.length() && zeile[zeile.length() - 1] == '\r') {
          zeile.remove(zeile.length() - 1);
        }
        return true;
      }
      if (zeile.length() < ZEILE_MAX) {
        zeile += c;
      }
    }
    if (!client.connected() && !client.available()) {
      return false;
    }
    delay(5);
    yield();
  }
  return false;
}

void sendeZeile(WiFiClient& client, const String& text) {
  client.print(text);
  client.print(F("\r\n"));
}

/**
 * @brief Eine Rumpfzeile stopfen und senden
 * @details Punkt-Stuffing bleibt auch bei HTML unverzichtbar: eine Zeile kann
 *          mit einem Punkt beginnen (CSS-Klasse, Fließtext), und der Server
 *          läse sie sonst als Ende der Nachricht (RFC 5321 4.5.2).
 */
/// Empfänger der Rumpfzeilen. Ein eigener Typ statt eines nackten
/// Clientzeigers: der Weg über void* muss auf genau denselben Typ
/// zurückcasten, sonst ist das Ergebnis undefiniert - bei einer Basisklasse
/// kann der Zeiger versetzt sein.
struct SendeZiel {
  BearSSL::WiFiClientSecure* client;
};

/**
 * @class WebPause
 * @brief Hält den Webserver für die Dauer des Versands an
 * @details Zwei Gründe. Erstens Platz: die Anfragepuffer werden frei. Zweitens
 *          und wichtiger die Ruhe - kommt während des TLS-Handshakes eine
 *          Anfrage herein, fordert der Webserver Speicher an, den es gerade
 *          nicht gibt. Zwei Versuche mit praktisch gleichem freien Heap gingen
 *          deshalb unterschiedlich aus: einer lief durch, der andere endete in
 *          Exception 29. Als Wächter, damit auch jeder Fehlerausgang aus
 *          sende() den Server wieder anschaltet.
 */
struct WebPause {
  WebPause() { WebManager::getInstance().pausiereWebserver(); }
  ~WebPause() { WebManager::getInstance().setzeWebserverFort(); }
  WebPause(const WebPause&) = delete;
  WebPause& operator=(const WebPause&) = delete;
};

void sendeRumpfZeile(const char* text, size_t length, void* context) {
  SendeZiel* ziel = static_cast<SendeZiel*>(context);
  BearSSL::WiFiClientSecure* client = ziel->client;

  // Ohne Zwischenpuffer: die Kette SendBody -> sendeRumpf -> leseAbschnitt ->
  // expandiereZeile -> hierher türmt schon genug auf dem 4-KB-Stack, und ein
  // weiterer 256-Byte-Puffer je Zeile brachte ihn zum Überlaufen.
  if (Smtp::needsStuffing(text, length)) {
    const uint8_t punkt = '.';
    client->write(&punkt, 1);
  }
  if (length > 0) {
    client->write(reinterpret_cast<const uint8_t*>(text), length);
  }
  client->write(reinterpret_cast<const uint8_t*>("\r\n"), 2);

  // Unbedingt yield(), nicht optimistic_yield(): der Rumpf ist die längste
  // ununterbrochene Arbeit im ganzen Versand.
  yield();
}

/**
 * @brief Kopfzeilen der Nachricht senden
 * @details Eigene Funktion, damit ihre Puffer den Stack wieder freigeben,
 *          bevor der Rumpf gestreamt wird.
 */
void sendeKopfzeilen(BearSSL::WiFiClientSecure& client, uint32_t jetzt, const String& von,
                     const String& an, const char* betreff, uint32_t laufendeNummer) {
  String kopf;
  char puffer[128];
  Mail::formatDate(jetzt, puffer, sizeof(puffer));
  kopf = String(F("Date: ")) + puffer + F("\r\n");
  kopf += String(F("From: ")) + von + F("\r\n");
  kopf += String(F("To: ")) + an + F("\r\n");

  // 256 statt 200: ein gefalteter Betreff braucht je Teilwort zwölf
  // Rahmenzeichen und drei für die Faltung (siehe Mail::encodeSubject).
  char betreffKodiert[256];
  if (Mail::encodeSubject(betreff, betreffKodiert, sizeof(betreffKodiert)) == 0) {
    strncpy(betreffKodiert, "Pflanzensensor", sizeof(betreffKodiert) - 1);
    betreffKodiert[sizeof(betreffKodiert) - 1] = '\0';
  }
  kopf += String(F("Subject: ")) + betreffKodiert + F("\r\n");

  char domaene[64];
  char messageId[128];
  if (Mail::domainOf(von.c_str(), domaene, sizeof(domaene)) > 0 &&
      Mail::formatMessageId(ConfigMgr.getDeviceName().c_str(), domaene, jetzt, laufendeNummer,
                            messageId, sizeof(messageId)) > 0) {
    kopf += String(F("Message-ID: ")) + messageId + F("\r\n");
  }

  kopf += F("MIME-Version: 1.0\r\n");
  // HTML statt Klartext: die Vorlagen bringen Farben, Emojis und eine Tabelle
  // mit. Kein multipart und kein Textteil - das bräuchte eine zweite Vorlage je
  // Mailart oder eine automatische Umwandlung, beides zuviel für ein Gerät mit
  // 15 KB freiem Heap.
  kopf += F("Content-Type: text/html; charset=utf-8\r\n");
  kopf += F("Content-Transfer-Encoding: 8bit\r\n");
  kopf += F("\r\n");
  client.print(kopf);
}

} // namespace

MailSender& MailSender::instance() {
  static MailSender sender;
  return sender;
}

void MailSender::begin() {
  reloadConfig();
  m_scheduler.restore(ConfigMgr.getMailLastWarning(), ConfigMgr.getMailLastAlive());
  if (!ConfigMgr.isMailBootEnabled()) {
    m_scheduler.skipBootMail();
  }
  LOG_INFO(F("Mail"), ConfigMgr.isMailEnabled() ? F("Mailversand aktiv") : F("Mailversand aus"));
}

void MailSender::reloadConfig() {
  Mail::SchedulerConfig c;
  c.enabled = ConfigMgr.isMailEnabled();
  c.bootMail = ConfigMgr.isMailBootEnabled();
  c.aliveMail = ConfigMgr.isMailAliveEnabled();
  c.warnIntervalSeconds = Mail::hoursToSeconds(ConfigMgr.getMailWarnHours());
  c.warnFrom = ConfigMgr.getMailWarnFrom() == 2 ? Mail::Level::Red : Mail::Level::Yellow;
  c.aliveIntervalSeconds = Mail::hoursToSeconds(ConfigMgr.getMailAliveHours());

  const uint32_t jetzt = static_cast<uint32_t>(Helper::getCurrentTime());
  if (m_lastAttempt == 0 && m_sent == 0) {
    m_scheduler.begin(c, jetzt);
    m_scheduler.restore(ConfigMgr.getMailLastWarning(), ConfigMgr.getMailLastAlive());
  } else {
    m_scheduler.setConfig(c);
  }
}

void MailSender::requestTestMail() {
  m_testPending = true;
  // Zwei Minuten Geduld bei Speichermangel. Der Webserver gibt seine Puffer
  // gleich nach der Antwort frei, und der Aufruf kommt ja gerade von dort -
  // ein Fehlschlag um ein paar hundert Byte wäre nur eine Frage des
  // Zeitpunkts, nicht der Einstellungen.
  m_testFristMs = millis() + TEST_GEDULD_MS;
}

void MailSender::aktualisiereZustaende() {
  if (!sensorManager) {
    return;
  }
  uint8_t kanal = 0;
  bool alleGemessen = true;
  for (const auto& sensor : sensorManager->getSensors()) {
    if (!sensor || !sensor->isEnabled()) {
      continue;
    }
    // Dieselbe Bedingung wie im SensorHandler (sensor_handler.cpp:231): ohne
    // Startzeit hat der Sensor noch nie erfolgreich gemessen, seine Werte wären
    // die Platzhalter aus dem Hochlauf.
    if (sensor->getMeasurementStartTime() == 0) {
      alleGemessen = false;
    }
    const SensorConfig& config = sensor->config();
    for (size_t i = 0; i < config.activeMeasurements; i++) {
      if (!config.measurements[i].enabled) {
        continue;
      }
      if (kanal >= Mail::Scheduler::MAX_CHANNELS) {
        break; // kein return: sonst bliebe die Bereitschaft unten ungemeldet
      }
      const String schluessel = sensor->getId() + "_" + String(i);
      m_scheduler.reportLevel(kanal, levelVon(sensor->getStatus(i)),
                              ConfigMgr.isMailSensorWatched(schluessel));
      kanal++;
    }
  }
  m_scheduler.setSensorsReady(alleGemessen);
}

bool MailSender::genugSpeicher(String& fehler) {
  meldeSpeicher(F("vor dem Aufraeumen"));

  // Den Handler-Cache des Webservers unbedingt leeren, nicht nur beim
  // Unterschreiten einer Schwelle: er ist der größte Posten, den wir
  // kurzfristig freigeben können, und baut sich beim nächsten Seitenaufruf von
  // selbst wieder auf. checkAndCleanup() täte hier nichts, weil der Heap für
  // seinen Maßstab noch reichlich wäre.
  MemoryMgr.emergencyCleanup();
  yield();

  meldeSpeicher(F("nach dem Aufraeumen"));

  const uint32_t frei = ESP.getFreeHeap();
  const uint32_t block = ESP.getMaxFreeBlockSize();
  if (frei < MIN_FREE_HEAP || block < MIN_FREE_BLOCK) {
    fehler = String(F("Zu wenig Speicher (")) + String(frei) + F(" B frei, groesster Block ") +
             String(block) + F(" B)");
    m_speichermangel = true;
    return false;
  }
  m_speichermangel = false;
  return true;
}

void MailSender::loop() {
  const uint32_t jetztMs = millis();
  // Eine angeforderte Testmail hat es eilig, darf die Schleife aber nicht mit
  // voller Geschwindigkeit belegen: sie kann auf freien Speicher warten müssen.
  // Die zwei Sekunden Verzug schaden nicht, im Gegenteil - der Webserver gibt
  // in der Zeit die Puffer der Antwort frei, die den Test ausgelöst hat.
  const uint32_t abstand = m_testPending ? TEST_ABSTAND_MS : PRUEFINTERVALL_MS;
  if ((jetztMs - m_lastCheck) < abstand) {
    return;
  }
  m_lastCheck = jetztMs;

  if (!ConfigMgr.isMailEnabled()) {
    m_testPending = false;
    return;
  }

  aktualisiereZustaende();

  const uint32_t jetzt = static_cast<uint32_t>(Helper::getCurrentTime());

  // Beim Start war womöglich noch keine Uhr da (NTP braucht ein paar Sekunden,
  // nach einem Neustart ohne Netz auch mal Minuten). Dann steht der
  // Startzeitpunkt des Planers auf null, und sobald die Uhr läuft, wäre das
  // Lebenszeichen "seit Jahrzehnten fällig" - es käme sofort.
  if (jetzt > 1600000000UL && m_scheduler.startedAt() < 1600000000UL) {
    m_scheduler.setStartedAt(jetzt);
    LOG_DEBUG(F("Mail"), F("Startzeitpunkt nachgetragen, nachdem die Uhr lief"));
  }
  Mail::Kind was = Mail::Kind::None;
  if (m_testPending) {
    was = Mail::Kind::Alive; // die Testmail ist inhaltlich ein Lebenszeichen
  } else {
    was = m_scheduler.due(jetzt);
  }
  if (was == Mail::Kind::None) {
    return;
  }

  String fehler;
  const bool istTest = m_testPending;
  m_testPending = false;
  m_lastAttempt = jetzt;

  if (sende(was, fehler)) {
    m_sent++;
    m_lastResult = F("Versand erfolgreich");
    LOG_INFO(F("Mail"), F("Mail verschickt"));
    if (!istTest) {
      m_scheduler.markSent(was, jetzt);
      // Zeitpunkte festhalten, damit die Sperren einen Neustart überdauern
      if (was == Mail::Kind::Warning) {
        ConfigMgr.setMailLastWarning(jetzt);
      } else if (was == Mail::Kind::Alive) {
        ConfigMgr.setMailLastAlive(jetzt);
      }
    }
  } else if (istTest && m_speichermangel && static_cast<int32_t>(m_testFristMs - jetztMs) > 0) {
    // Der Speicher ist ein Momentzustand, keine falsche Einstellung: gleich
    // noch einmal versuchen. Wichtig ist, dass der Versuch überhaupt
    // stattfindet - erst er räumt den Handler-Cache des Webservers leer, und
    // das sind die entscheidenden Kilobyte. Eine Vorabprüfung an dieser Stelle
    // hätte genau das verhindert und die Testmail dauerhaft blockiert.
    m_testPending = true;
    m_lastResult = String(F("Warte auf freien Speicher - ")) + fehler;
    LOG_DEBUG(F("Mail"), m_lastResult);
  } else {
    m_lastResult = fehler;
    LOG_ERROR(F("Mail"), String(F("Versand fehlgeschlagen: ")) + fehler);
    // Auch ein Fehlschlag setzt die Sperre: sonst versucht das Gerät es bei
    // falschem Passwort alle 30 Sekunden erneut und der Server sperrt uns aus.
    if (!istTest && was != Mail::Kind::Boot) {
      m_scheduler.markSent(was, jetzt);
    } else if (!istTest) {
      m_scheduler.skipBootMail();
    }
  }
}

bool MailSender::sende(Mail::Kind kind, String& fehler) {
  const String host = ConfigMgr.getMailHost();
  const uint16_t port = ConfigMgr.getMailPort();
  const String von = ConfigMgr.getMailFrom();
  const String an = ConfigMgr.getMailTo();

  if (host.length() == 0 || !Mail::looksLikeAddress(an.c_str())) {
    fehler = F("Server oder Empfaengeradresse fehlt");
    return false;
  }
  if (!Mail::looksLikeAddress(von.c_str())) {
    fehler = F("Absenderadresse unbrauchbar");
    return false;
  }
  const uint32_t jetzt = static_cast<uint32_t>(Helper::getCurrentTime());
  if (jetzt < 1600000000UL) {
    fehler = F("Keine Uhrzeit - die Datumskopfzeile waere falsch");
    return false;
  }
  if (!genugSpeicher(fehler)) {
    return false;
  }

  // Betreff jetzt, der Rumpf erst beim Senden: der Betreff muss in die
  // Kopfzeilen, der Rumpf wird zeilenweise aus der Vorlagendatei gestreamt und
  // liegt damit nie ganz im Heap.
  char betreff[MailVorlage::BETREFF_MAX + 1];
  if (MailVorlagen::betreff(kind, betreff, sizeof(betreff)) == 0) {
    strncpy(betreff, "Pflanzensensor", sizeof(betreff) - 1);
    betreff[sizeof(betreff) - 1] = '\0';
  }

  // Ohne Zertifikatsprüfung. Sie war schon vorher fast nie möglich: sie kostet
  // gut 5 KB mehr Heap, im laufenden Betrieb stand das nie zur Verfügung, und
  // der Versand fiel jedes Mal auf den ungeprüften Weg zurück - der Code tat
  // also nur so, als prüfe er. Dazu ließ sich ohnehin nur eine Let's-Encrypt-
  // Kette prüfen; bei jedem anderen Anbieter war der erste Versuch verlorene
  // Zeit und eine zerstückelte Halde. Die Zugangsdaten sind trotzdem
  // verschlüsselt unterwegs, nur die Echtheit des Servers wird nicht geprüft.
  // Ab hier schweigt der Webserver. Bewusst erst hier und nicht schon vor der
  // Speicherprüfung: die läuft bei einer wartenden Testmail alle paar Sekunden,
  // und ein Server, der sich im Sekundentakt ab- und anmeldet, ist für den
  // Browser schlicht nicht erreichbar. Genau das war er zwischenzeitlich.
  const WebPause webPause;
  yield();

  BearSSL::WiFiClientSecure tlsClient;
  tlsClient.setInsecure();
  // 512-Byte-Puffer statt der üblichen 16 KB. Ohne das passt TLS auf diesem
  // Gerät nicht in den Heap; der Server muss dafür die Aushandlung nach
  // RFC 6066 beherrschen (datenkollektiv und posteo tun das).
  tlsClient.setBufferSizes(512, 512);
  tlsClient.setTimeout(ANTWORT_FRIST_MS);

  if (!tlsClient.connect(host.c_str(), port)) {
    char text[96];
    tlsClient.getLastSSLError(text, sizeof(text));
    fehler = String(F("Verbindung fehlgeschlagen: ")) + text;
    return false;
  }
  LOG_INFO(F("Mail"), F("Verbunden, Zertifikat nicht geprueft"));
  // Der Punkt mit dem geringsten Vorrat im ganzen Versand: die TLS-Puffer
  // stehen, der Rumpf kommt erst danach und wird gestreamt.
  meldeSpeicher(F("nach dem Handshake"));

  const uint32_t restNachHandshake = ESP.getFreeHeap();
  if (restNachHandshake < MIN_FREE_NACH_HANDSHAKE) {
    tlsClient.stop();
    fehler = String(F("Nach dem Handshake nur noch ")) + String(restNachHandshake) +
             F(" B frei - abgebrochen, bevor etwas abstuerzt");
    m_speichermangel = true;
    return false;
  }

  Smtp::Config sc;
  sc.helo = "pflanzensensor";
  sc.user = ConfigMgr.getMailUser().length() ? ConfigMgr.getMailUser().c_str() : nullptr;
  sc.password = ConfigMgr.getMailPassword().c_str();
  sc.from = von.c_str();
  sc.to = an.c_str();
  // Port 587 beginnt im Klartext und wird per STARTTLS gesichert. Diese
  // Umsetzung baut TLS von Anfang an auf (Port 465) - für 587 müsste der
  // Client mitten in der Sitzung umschalten, was WiFiClientSecure nicht kann.
  sc.useStartTls = false;

  if (ConfigMgr.isDebugMail()) {
    LOG_DEBUG(F("Mail"), String(F("Anmeldung als: ")) +
                             (sc.user ? ConfigMgr.getMailUser() : String(F("(ohne Anmeldung)"))) +
                             F(", Absender: ") + von + F(", Empfaenger: ") + an);
  }

  Smtp::Session sitzung;
  sitzung.begin(sc);

  bool fertig = false;
  String zeile;
  while (!fertig) {
    if (!leseZeile(tlsClient, zeile)) {
      fehler = F("Keine Antwort vom Server");
      tlsClient.stop();
      return false;
    }
    // Den Serverdialog mitschreiben, wenn "Debug Mail" eingeschaltet ist. Ohne
    // das bleibt bei einer abgelehnten Anmeldung nur die Zahl 535, und die sagt
    // niemandem, ob Benutzername, Passwort oder das Anmeldeverfahren nicht
    // passt. Passwörter stehen hier nicht drin: die base64-Zeilen sendet das
    // Gerät, sie kommen nicht zurück.
    if (ConfigMgr.isDebugMail()) {
      LOG_DEBUG(F("Mail"), String(F("< ")) + zeile);
    }

    const Smtp::Step schritt = sitzung.feedLine(zeile.c_str());
    switch (schritt) {
    case Smtp::Step::Wait:
      break;
    case Smtp::Step::Send: {
      // Kommandos mitschreiben, aber nie den base64-Block einer Anmeldung -
      // darin steckt das Passwort.
      const String kommando = sitzung.command();
      const bool geheim =
          (sitzung.phase() == Smtp::Phase::AuthUser || sitzung.phase() == Smtp::Phase::AuthPass ||
           sitzung.phase() == Smtp::Phase::AuthPlain);
      LOG_DEBUG(F("Mail"), String(F("> ")) + (geheim ? String(F("<Zugangsdaten>")) : kommando));
      sendeZeile(tlsClient, kommando);
      break;
    }
    case Smtp::Step::StartTls:
      fehler = F("STARTTLS wird nicht unterstuetzt - bitte Port 465 verwenden");
      tlsClient.stop();
      return false;
    case Smtp::Step::SendBody: {
      // Kopfzeilen in einer eigenen Funktion: ihre Puffer (Betreff kodiert,
      // Domäne, Message-ID) sind zusammen fast ein halbes Kilobyte, und ihr
      // Stackrahmen muss weg sein, bevor das Streamen beginnt.
      sendeKopfzeilen(tlsClient, jetzt, von, an, betreff, m_sent);

      if (!sitzung.supports8BitMime()) {
        // Kein Abbruchgrund - praktisch jeder Server kann es. Aber es gehört
        // ins Log, statt still zu hoffen.
        LOG_WARN(F("Mail"), F("Server bietet kein 8BITMIME an - Umlaute koennten leiden"));
      }

      // Rumpf zeilenweise aus der Vorlage. Die Datei wird erst hier geöffnet
      // und gleich wieder geschlossen: ein offenes Dateihandle hält einen
      // Cachepuffer, der nicht mit der Handshake-Spitze zusammenfallen darf.
      //
      // Rahmen und Stil kommen von hier, nicht aus der Vorlage: im Vorlagentext
      // steht kein HTML mehr, das jemand aufmachen und zu schließen vergessen
      // könnte.
      SendeZiel ziel{&tlsClient};
      static const char AUF[] = "<!DOCTYPE html><html><head>"
                                "<meta charset=\"utf-8\">"
                                "<meta name=\"viewport\" content=\"width=device-width\">"
                                "<style>";
      sendeRumpfZeile(AUF, strlen(AUF), &ziel);
      MailVorlagen::sendeStil(sendeRumpfZeile, &ziel);
      static const char MITTE[] = "</style></head><body>";
      sendeRumpfZeile(MITTE, strlen(MITTE), &ziel);

      MailVorlagen::sendeRumpf(kind, sendeRumpfZeile, &ziel);

      static const char ZU[] = "</body></html>";
      sendeRumpfZeile(ZU, strlen(ZU), &ziel);

      tlsClient.print(F(".\r\n"));
      break;
    }
    case Smtp::Step::Done:
      fertig = true;
      break;
    case Smtp::Step::Failed:
      if (sitzung.phase() == Smtp::Phase::Failed && sitzung.lastCode() == 535) {
        LOG_ERROR(F("Mail"), String(F("Server bot an: LOGIN=")) +
                                 (sitzung.supportsAuthLogin() ? F("ja") : F("nein")) +
                                 F(", PLAIN=") +
                                 (sitzung.supportsAuthPlain() ? F("ja") : F("nein")));
      }
      fehler = sitzung.error();
      tlsClient.stop();
      return false;
    }
    yield();
  }

  tlsClient.stop();
  meldeSpeicher(F("nach dem Versand"));
  return true;
}
