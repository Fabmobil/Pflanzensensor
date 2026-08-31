/**
 * @file smtp_session.h
 * @brief Ablauf einer SMTP-Sitzung, ohne Netzwerk und ohne Hardware
 * @details Header-only und frei von Arduino-Abhängigkeiten - aus demselben
 *          Grund wie utils/chronik_format.h: so lässt sich der Ablauf nativ
 *          testen, ohne dass ein Mailserver oder ein Gerät im Spiel ist. Die
 *          Klasse kennt keine Sockets; sie bekommt Antwortzeilen gefüttert und
 *          sagt, was als nächstes zu senden ist.
 *
 *          Genau hier liegen die Fallstricke von SMTP, die man beim Ausprobieren
 *          gegen einen echten Server nur zufällig trifft: mehrzeilige Antworten,
 *          bei denen erst die Zeile mit Leerzeichen nach dem Code das Ende
 *          markiert; die Unterscheidung zwischen vorübergehendem (4xx) und
 *          endgültigem (5xx) Fehler; und das Punkt-Stuffing im Nachrichtenrumpf,
 *          ohne das eine Zeile mit einem Punkt die Nachricht vorzeitig beendet.
 */

#ifndef SMTP_SESSION_H
#define SMTP_SESSION_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace Smtp {

/// Base64 nach RFC 4648, für AUTH LOGIN.
/// @return Länge der Ausgabe ohne Nullbyte, 0 wenn der Puffer zu klein ist
inline size_t base64Encode(const char* input, size_t length, char* out, size_t outSize) {
  static const char TABELLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const size_t noetig = ((length + 2) / 3) * 4;
  if (!out || outSize < noetig + 1) {
    return 0;
  }
  size_t at = 0;
  for (size_t i = 0; i < length; i += 3) {
    const uint32_t a = static_cast<uint8_t>(input[i]);
    const uint32_t b = (i + 1 < length) ? static_cast<uint8_t>(input[i + 1]) : 0;
    const uint32_t c = (i + 2 < length) ? static_cast<uint8_t>(input[i + 2]) : 0;
    const uint32_t block = (a << 16) | (b << 8) | c;
    out[at++] = TABELLE[(block >> 18) & 0x3F];
    out[at++] = TABELLE[(block >> 12) & 0x3F];
    out[at++] = (i + 1 < length) ? TABELLE[(block >> 6) & 0x3F] : '=';
    out[at++] = (i + 2 < length) ? TABELLE[block & 0x3F] : '=';
  }
  out[at] = '\0';
  return at;
}

/**
 * @brief Antwortcode einer Zeile, oder 0 wenn die Zeile keiner ist
 * @details SMTP-Antworten beginnen mit drei Ziffern. Danach steht ein
 *          Leerzeichen (letzte Zeile) oder ein Bindestrich (es folgt noch was).
 */
inline int replyCode(const char* line) {
  if (!line || strlen(line) < 3) {
    return 0;
  }
  for (uint8_t i = 0; i < 3; i++) {
    if (line[i] < '0' || line[i] > '9') {
      return 0;
    }
  }
  return (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
}

/// @brief Ist das die letzte Zeile einer Antwort?
/// @details "250-STARTTLS" heißt: es kommt noch mehr. "250 OK" beendet sie.
inline bool isFinalLine(const char* line) {
  if (!line || strlen(line) < 4) {
    return true; // zu kurz für eine Fortsetzung - als Ende behandeln
  }
  return line[3] != '-';
}

/// Was der Aufrufer als nächstes tun soll.
enum class Step : uint8_t {
  Wait,     ///< auf weitere Antwortzeilen warten
  Send,     ///< command() senden
  StartTls, ///< TLS aushandeln, danach weiter
  SendBody, ///< den Nachrichtenrumpf senden, dann feedLine() mit der Antwort
  Done,     ///< fertig, Verbindung schließen
  Failed    ///< abgebrochen, error() erklärt warum
};

enum class Phase : uint8_t {
  Greeting,
  Ehlo,
  StartTls,
  EhloAfterTls,
  AuthPlain,
  AuthLogin,
  AuthUser,
  AuthPass,
  MailFrom,
  RcptTo,
  Data,
  Body,
  Quit,
  Done,
  Failed
};

struct Config {
  const char* helo{"pflanzensensor"};
  const char* user{nullptr}; ///< nullptr = ohne Anmeldung
  const char* password{nullptr};
  const char* from{nullptr};
  const char* to{nullptr};
  /// true: Verbindung startet unverschlüsselt und wird per STARTTLS gesichert
  /// (Port 587). false: TLS steht schon (Port 465).
  bool useStartTls{false};
};

/**
 * @class Session
 * @brief Führt durch EHLO, Anmeldung, Umschläge und Rumpf
 */
class Session {
public:
  static constexpr size_t COMMAND_MAX = 200;

  void begin(const Config& config) {
    m_config = config;
    m_phase = Phase::Greeting;
    m_command[0] = '\0';
    m_error[0] = '\0';
    m_lastCode = 0;
    m_authLogin = false;
    m_authPlain = false;
    m_eightBit = false;
    m_lastText[0] = '\0';
  }

  Phase phase() const { return m_phase; }
  int lastCode() const { return m_lastCode; }
  /// Letzte Antwortzeile im Klartext. Bei einem 535 steht hier, was der Server
  /// wirklich bemängelt - ohne das bleibt nur Raten.
  const char* lastText() const { return m_lastText; }
  const char* command() const { return m_command; }
  const char* error() const { return m_error; }
  /// Bot der Server AUTH an? Erst nach dem EHLO aussagekräftig.
  bool authOffered() const { return m_authLogin || m_authPlain; }
  bool supportsAuthLogin() const { return m_authLogin; }
  bool supportsAuthPlain() const { return m_authPlain; }
  /// Bot der Server 8BITMIME an? Für Mails mit Umlauten und Emojis relevant.
  bool supports8BitMime() const { return m_eightBit; }

  /**
   * @brief Eine Antwortzeile des Servers verarbeiten
   * @return Was als nächstes zu tun ist
   */
  Step feedLine(const char* line) {
    const int code = replyCode(line);
    if (code == 0) {
      // Keine Antwortzeile - im EHLO-Block kommen auch Fortsetzungen ohne
      // Code vor; alles andere ignorieren wir stillschweigend.
      return Step::Wait;
    }
    m_lastCode = code;
    strncpy(m_lastText, line, sizeof(m_lastText) - 1);
    m_lastText[sizeof(m_lastText) - 1] = '\0';

    // Erweiterungen aus dem EHLO-Block mitlesen, bevor die Zeile abgehakt wird.
    // LOGIN und PLAIN werden getrennt vermerkt: manche Server bieten nur eines
    // von beiden an, und ein Anmeldeversuch mit dem falschen Verfahren endet
    // mit demselben 535 wie ein falsches Passwort.
    if (m_phase == Phase::Ehlo || m_phase == Phase::EhloAfterTls) {
      if (enthaelt(line, "AUTH")) {
        if (enthaelt(line, "LOGIN")) {
          m_authLogin = true;
        }
        if (enthaelt(line, "PLAIN")) {
          m_authPlain = true;
        }
      }
      if (enthaelt(line, "8BITMIME")) {
        m_eightBit = true;
      }
    }

    if (!isFinalLine(line)) {
      return Step::Wait; // mehrzeilige Antwort, das Ende steht noch aus
    }

    return advance(code);
  }

private:
  static bool enthaelt(const char* heuhaufen, const char* nadel) {
    return heuhaufen && nadel && strstr(heuhaufen, nadel) != nullptr;
  }

  void fail(const char* text) {
    m_phase = Phase::Failed;
    strncpy(m_error, text, sizeof(m_error) - 1);
    m_error[sizeof(m_error) - 1] = '\0';
  }

  /// Erwarteten Code prüfen; bei Abweichung mit sprechendem Fehler abbrechen.
  bool erwarte(int code, int soll, const char* wobei) {
    if (code / 100 == soll / 100) {
      return true;
    }
    char text[sizeof(m_error)];
    // 4xx ist vorübergehend (Server voll, Graylisting), 5xx endgültig
    // (Anmeldung falsch, Empfänger unbekannt). Der Unterschied entscheidet, ob
    // ein späterer Versuch Sinn hat - deshalb der Code. Der Klartext dahinter
    // ist das, was tatsächlich weiterhilft: "authentication failed" liest sich
    // anders als "Invalid user".
    snprintf(text, sizeof(text), "%s: %s", wobei, m_lastText);
    fail(text);
    return false;
  }

  void setCommand(const char* format, const char* wert = nullptr) {
    if (wert) {
      snprintf(m_command, sizeof(m_command), format, wert);
    } else {
      strncpy(m_command, format, sizeof(m_command) - 1);
      m_command[sizeof(m_command) - 1] = '\0';
    }
  }

  Step advance(int code) {
    switch (m_phase) {
    case Phase::Greeting:
      if (!erwarte(code, 220, "Begruessung"))
        return Step::Failed;
      m_phase = Phase::Ehlo;
      setCommand("EHLO %s", m_config.helo);
      return Step::Send;

    case Phase::Ehlo:
      if (!erwarte(code, 250, "EHLO"))
        return Step::Failed;
      if (m_config.useStartTls) {
        m_phase = Phase::StartTls;
        setCommand("STARTTLS");
        return Step::Send;
      }
      return nachAnmeldung();

    case Phase::StartTls:
      if (!erwarte(code, 220, "STARTTLS"))
        return Step::Failed;
      // Nach dem Umschalten muss EHLO wiederholt werden: die Erweiterungsliste
      // vor TLS ist bewusst nicht vertrauenswürdig, viele Server bieten AUTH
      // auch erst danach an.
      m_phase = Phase::EhloAfterTls;
      m_authLogin = false;
      m_authPlain = false;
      setCommand("EHLO %s", m_config.helo);
      return Step::StartTls;

    case Phase::EhloAfterTls:
      if (!erwarte(code, 250, "EHLO nach STARTTLS"))
        return Step::Failed;
      return nachAnmeldung();

    case Phase::AuthPlain:
      if (!erwarte(code, 235, "Anmeldung"))
        return Step::Failed;
      return mailFrom();

    case Phase::AuthLogin:
      if (code != 334) {
        fail("AUTH LOGIN abgelehnt");
        return Step::Failed;
      }
      m_phase = Phase::AuthUser;
      if (!base64InCommand(m_config.user)) {
        fail("Benutzername zu lang");
        return Step::Failed;
      }
      return Step::Send;

    case Phase::AuthUser:
      if (code != 334) {
        fail("Benutzername abgelehnt");
        return Step::Failed;
      }
      m_phase = Phase::AuthPass;
      if (!base64InCommand(m_config.password)) {
        fail("Passwort zu lang");
        return Step::Failed;
      }
      return Step::Send;

    case Phase::AuthPass:
      if (!erwarte(code, 235, "Anmeldung"))
        return Step::Failed;
      return mailFrom();

    case Phase::MailFrom:
      if (!erwarte(code, 250, "MAIL FROM"))
        return Step::Failed;
      m_phase = Phase::RcptTo;
      setCommand("RCPT TO:<%s>", m_config.to);
      return Step::Send;

    case Phase::RcptTo:
      if (!erwarte(code, 250, "RCPT TO"))
        return Step::Failed;
      m_phase = Phase::Data;
      setCommand("DATA");
      return Step::Send;

    case Phase::Data:
      if (code != 354) {
        fail("DATA abgelehnt");
        return Step::Failed;
      }
      m_phase = Phase::Body;
      m_command[0] = '\0';
      return Step::SendBody;

    case Phase::Body:
      if (!erwarte(code, 250, "Nachricht"))
        return Step::Failed;
      m_phase = Phase::Quit;
      setCommand("QUIT");
      return Step::Send;

    case Phase::Quit:
      m_phase = Phase::Done;
      return Step::Done;

    default:
      return Step::Failed;
    }
  }

  Step nachAnmeldung() {
    if (!m_config.user || !m_config.user[0]) {
      return mailFrom();
    }
    if (!authOffered()) {
      fail("Server bietet keine Anmeldung an");
      return Step::Failed;
    }

    // LOGIN zuerst, weil es der verbreitetere Weg ist; PLAIN nur, wenn der
    // Server ausdrücklich nichts anderes anbietet.
    if (m_authLogin) {
      m_phase = Phase::AuthLogin;
      setCommand("AUTH LOGIN");
      return Step::Send;
    }

    m_phase = Phase::AuthPlain;
    if (!plainInCommand()) {
      fail("Zugangsdaten zu lang für AUTH PLAIN");
      return Step::Failed;
    }
    return Step::Send;
  }

  Step mailFrom() {
    m_phase = Phase::MailFrom;
    setCommand("MAIL FROM:<%s>", m_config.from);
    return Step::Send;
  }

  /**
   * @brief "AUTH PLAIN <base64>" bauen
   * @details Das Argument ist \0benutzer\0passwort - die Nullbytes gehören zum
   *          Format (RFC 4616) und dürfen nicht als Zeichenkettenende
   *          missverstanden werden, deshalb die Längenrechnung von Hand.
   */
  bool plainInCommand() {
    const char* user = m_config.user ? m_config.user : "";
    const char* pass = m_config.password ? m_config.password : "";
    char roh[128];
    const size_t userLen = strlen(user);
    const size_t passLen = strlen(pass);
    const size_t gesamt = 1 + userLen + 1 + passLen;
    if (gesamt > sizeof(roh)) {
      return false;
    }
    roh[0] = '\0';
    memcpy(roh + 1, user, userLen);
    roh[1 + userLen] = '\0';
    memcpy(roh + 2 + userLen, pass, passLen);

    // Der Kodierpuffer muss den Platz für "AUTH PLAIN " freilassen, sonst
    // könnte das Kommando abgeschnitten werden - und ein halbes Passwort
    // ergäbe eine Fehlermeldung, die niemand deuten kann.
    static constexpr size_t PRAEFIX_LAENGE = 11; // "AUTH PLAIN "
    char kodiert[COMMAND_MAX - PRAEFIX_LAENGE];
    if (base64Encode(roh, gesamt, kodiert, sizeof(kodiert)) == 0) {
      return false;
    }
    snprintf(m_command, sizeof(m_command), "AUTH PLAIN %s", kodiert);
    return true;
  }

  bool base64InCommand(const char* wert) {
    if (!wert) {
      wert = "";
    }
    return base64Encode(wert, strlen(wert), m_command, sizeof(m_command)) > 0 || wert[0] == '\0';
  }

  Config m_config;
  Phase m_phase{Phase::Greeting};
  int m_lastCode{0};
  bool m_authLogin{false};
  bool m_authPlain{false};
  bool m_eightBit{false};
  char m_command[COMMAND_MAX]{};
  char m_lastText[80]{};
  char m_error[112]{};
};

/**
 * @brief Muss diese Zeile gestopft werden?
 * @details Für Aufrufer, die die Zeile nicht kopieren können - auf dem ESP8266
 *          ist der Stack nur 4 KB groß, und ein 256-Byte-Puffer je Zeile war
 *          dort einmal zuviel. Dieselbe Regel wie stuffLine(), nur ohne Puffer.
 */
inline bool needsStuffing(const char* line, size_t length) {
  return line && length > 0 && line[0] == '.';
}

/**
 * @brief Eine Rumpfzeile punkt-gestopft ausgeben
 * @return Länge der Ausgabe ohne Nullbyte, 0 wenn der Puffer zu klein ist
 * @details Eine Zeile, die mit einem Punkt beginnt, muss verdoppelt werden -
 *          sonst liest der Server sie als das Ende der Nachricht (RFC 5321,
 *          4.5.2). Genau daran zerbrechen Nachrichten, die zufällig eine Zeile
 *          mit einem Punkt beginnen.
 */
inline size_t stuffLine(const char* line, char* out, size_t outSize) {
  if (!line || !out) {
    return 0;
  }
  const size_t length = strlen(line);
  const bool stopfen = (length > 0 && line[0] == '.');
  const size_t noetig = length + (stopfen ? 1 : 0);
  if (outSize < noetig + 1) {
    return 0;
  }
  size_t at = 0;
  if (stopfen) {
    out[at++] = '.';
  }
  memcpy(out + at, line, length);
  at += length;
  out[at] = '\0';
  return at;
}

} // namespace Smtp

#endif // SMTP_SESSION_H
