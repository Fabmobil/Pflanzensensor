/**
 * @file mail_service.cpp
 */
#include "services/mail_service.h"

#include <stdio.h>
#include <stdlib.h>

#include "core/mail/envelope.h"
#include "core/util/format.h"
#include "hal/crypto.h"
#include "hal/system.h"
#include "services/log.h"

namespace ps {

namespace {

Mail::Level levelOf(ChannelStatus s) {
  switch (s) {
  case ChannelStatus::Green:
    return Mail::Level::Green;
  case ChannelStatus::Yellow:
    return Mail::Level::Yellow;
  case ChannelStatus::Red:
    return Mail::Level::Red;
  default:
    return Mail::Level::Unknown;
  }
}

const char* ampel(ChannelStatus s) {
  switch (s) {
  case ChannelStatus::Green:
    return "\xF0\x9F\x9F\xA2";
  case ChannelStatus::Yellow:
    return "\xF0\x9F\x9F\xA1";
  case ChannelStatus::Red:
    return "\xF0\x9F\x94\xB4";
  default:
    return "\xE2\x9A\xAA";
  }
}

/// Puffert kleine Schreibvorgänge, damit nicht jedes Stück ein TCP-Segment wird
class BufferedSink : public Sink {
public:
  BufferedSink(Sink& out, char* buf, size_t cap) : out_(out), buf_(buf), cap_(cap) {}
  ~BufferedSink() { flush(); }
  void write(const char* d, size_t n) override {
    while (n) {
      const size_t take = n < cap_ - len_ ? n : cap_ - len_;
      memcpy(buf_ + len_, d, take);
      len_ += take;
      d += take;
      n -= take;
      if (len_ == cap_)
        flush();
    }
  }
  using Sink::write;
  void flush() {
    if (len_)
      out_.write(buf_, len_);
    len_ = 0;
  }

private:
  Sink& out_;
  char* buf_;
  size_t cap_;
  size_t len_ = 0;
};

/// Arbeitsspeicher des Versands (liegt hinter dem Klartextpuffer, nicht auf dem Stack)
struct Scratch {
  char subject[mailbot::kMaxSubject + 1];
  uint8_t key[40];
  char host[64];
  char path[64];
  char aad[64];
  char mac[18];
  char head[320];
  char sink[256];
};

/// Werte der Platzhalter, einmal je Mail eingesammelt
struct Values {
  char geraet[33], name[72], ip[16], ssid[33], neustarts[12], laufzeit[24], datum[16], uhrzeit[12];
  MailVorlage::Paar paare[8];
};

struct BlockCtx {
  const ConfigService* cfg;
  const SensorService* sensors;
};

void blockLines(const char* name, MailVorlage::ZeilenSenke aus, void* senkeCtx, void* ctx) {
  const BlockCtx& b = *static_cast<BlockCtx*>(ctx);
  const bool onlyAlarm = strcmp(name, MailVorlage::BLOCK_AUFFAELLIGE) == 0;
  const DeviceConfig& d = b.cfg->device();
  const Mail::Level warnFrom = d.mailWarnFrom == 1 ? Mail::Level::Red : Mail::Level::Yellow;
  const Instant now = hal::now();
  const char* open = "<table class=\"werte\">";
  aus(open, strlen(open), senkeCtx);
  bool any = false;
  for (uint8_t s = 0; s < board::kSlotCount; s++) {
    const ChannelConfig& cc = b.cfg->sensors().channels[s];
    if (!cc.enabled || !b.cfg->sensors().sensors[board::kSlots[s].sensor].enabled)
      continue;
    const ChannelStatus st = b.sensors->status(s, now);
    char key[32];
    schema::keyOfSlot(s, key, sizeof(key));
    if (onlyAlarm &&
        (!Mail::istAuffaellig(levelOf(st), warnFrom) || !Mail::isWatched(d.mailWatch.c_str(), key)))
      continue;
    char val[32] = "--";
    const float v = b.sensors->channel(s).value;
    if (!isnan(v))
      snprintf(val, sizeof(val), "%.1f%s", static_cast<double>(v), cc.unit.c_str());
    char line[MailVorlage::ZEILE_MAX + 1];
    snprintf(line, sizeof(line), "<tr><td class=\"name\">%s %s</td><td class=\"wert\">%s</td></tr>",
             ampel(st), cc.name.c_str(), val);
    aus(line, strlen(line), senkeCtx);
    any = true;
  }
  if (onlyAlarm && !any) {
    const char* ok = "<tr><td class=\"name\" colspan=\"2\">\xE2\x9C\x85 Jetzt ist wieder alles im "
                     "grünen Bereich!</td></tr>";
    aus(ok, strlen(ok), senkeCtx);
  }
  const char* close = "</table>";
  aus(close, strlen(close), senkeCtx);
}

void lineToSink(const char* text, size_t len, void* ctx) {
  Sink* s = static_cast<Sink*>(ctx);
  s->write(text, len);
  s->write("\n", 1);
}

struct Expand {
  MailVorlage::Umgebung* u;
  Sink* out;
};

bool expandLine(const char* line, void* ctx) {
  Expand& e = *static_cast<Expand*>(ctx);
  MailVorlage::expandiereZeile(line, *e.u, lineToSink, e.out);
  return true;
}

bool styleLine(const char* line, void* ctx) {
  Sink* s = static_cast<Sink*>(ctx);
  s->write(line);
  s->write("\n", 1);
  return true;
}

void collect(Values& v, const ConfigService& cfg, NetService& net, const SystemInfo& sys) {
  strncpy(v.geraet, cfg.device().deviceName.c_str(), sizeof(v.geraet) - 1);
  v.geraet[sizeof(v.geraet) - 1] = '\0';
  snprintf(v.name, sizeof(v.name), "%s.local", net.hostname());
  net.ip(v.ip, sizeof(v.ip));
  net.ssid(v.ssid, sizeof(v.ssid));
  snprintf(v.neustarts, sizeof(v.neustarts), "%lu", static_cast<unsigned long>(sys.rebootCount()));
  fmt::uptime(sys.uptimeS(), v.laufzeit, sizeof(v.laufzeit));
  hal::LocalTime lt;
  if (hal::toLocalTime(hal::epoch(), lt)) {
    fmt::date(lt.year, lt.month, lt.day, v.datum, sizeof(v.datum));
    fmt::time(lt.hour, lt.minute, v.uhrzeit, sizeof(v.uhrzeit));
  } else {
    strcpy(v.datum, "???");
    strcpy(v.uhrzeit, "???");
  }
  v.paare[0] = {"geraet", v.geraet};
  v.paare[1] = {"name", v.name};
  v.paare[2] = {"ip", v.ip};
  v.paare[3] = {"ssid", v.ssid};
  v.paare[4] = {"neustarts", v.neustarts};
  v.paare[5] = {"laufzeit", v.laufzeit};
  v.paare[6] = {"datum", v.datum};
  v.paare[7] = {"uhrzeit", v.uhrzeit};
}

/// "http://host:port/pfad" zerlegen
bool parseUrl(const char* url, char* host, size_t hostCap, uint16_t& port, char* path,
              size_t pathCap) {
  if (strncmp(url, "http://", 7) != 0)
    return false;
  const char* p = url + 7;
  const char* slash = strchr(p, '/');
  const char* end = slash ? slash : p + strlen(p);
  const char* colon = static_cast<const char*>(memchr(p, ':', static_cast<size_t>(end - p)));
  const size_t hlen = static_cast<size_t>((colon ? colon : end) - p);
  if (hlen == 0 || hlen >= hostCap)
    return false;
  memcpy(host, p, hlen);
  host[hlen] = '\0';
  port = colon ? static_cast<uint16_t>(atoi(colon + 1)) : 80;
  if (port == 0)
    return false;
  size_t plen = slash ? strlen(slash) : 0;
  while (plen > 0 && slash[plen - 1] == '/')
    plen--;
  if (plen + 10 >= pathCap)
    return false;
  if (plen)
    memcpy(path, slash, plen);
  strcpy(path + plen, "/send.php");
  return true;
}

} // namespace

Mail::SchedulerConfig MailService::schedulerConfig() const {
  const DeviceConfig& d = cfg_.device();
  Mail::SchedulerConfig c;
  c.enabled = d.mailEnabled;
  c.bootMail = d.mailBoot;
  c.aliveMail = d.mailAlive;
  c.warnIntervalSeconds = Mail::hoursToSeconds(d.mailWarnHours);
  c.aliveIntervalSeconds = Mail::hoursToSeconds(d.mailAliveHours);
  c.warnFrom = d.mailWarnFrom == 1 ? Mail::Level::Red : Mail::Level::Yellow;
  return c;
}

void MailService::begin(Instant now) {
  sched_.begin(schedulerConfig(), hal::epoch());
  sched_.restore(state_.mailLastWarning(), state_.mailLastAlive());
  if (!cfg_.device().mailBoot)
    sched_.skipBootMail();
  lastCheck_ = now;
  LOGI(Mod::Mail, "%s", cfg_.device().mailEnabled ? "Mailversand aktiv" : "Mailversand aus");
}

void MailService::configChanged() { sched_.setConfig(schedulerConfig()); }

void MailService::requestTest(Instant now) {
  testPending_ = true;
  testDeadline_ = now + Duration::ms(kTestPatienceMs);
  lastResult_ = "Testmail angefordert";
}

void MailService::updateLevels() {
  const Instant now = hal::now();
  const DeviceConfig& d = cfg_.device();
  sched_.clearLevels();
  for (uint8_t s = 0; s < board::kSlotCount && s < Mail::Scheduler::MAX_CHANNELS; s++) {
    const ChannelConfig& cc = cfg_.sensors().channels[s];
    if (!cc.enabled)
      continue;
    char key[32];
    schema::keyOfSlot(s, key, sizeof(key));
    sched_.reportLevel(s, levelOf(sensors_.status(s, now)),
                       Mail::isWatched(d.mailWatch.c_str(), key));
  }
  sched_.setSensorsReady(sensors_.allMeasuredOnce());
}

void MailService::tick(Instant now) {
  if (phase_ == Phase::Waiting) {
    pollResponse(now);
    return;
  }
  const uint32_t every = testPending_ ? kTestCheckMs : kCheckMs;
  if (checkedOnce_ && (now - lastCheck_).millis() < static_cast<int32_t>(every))
    return;
  checkedOnce_ = true;
  lastCheck_ = now;
  if (!cfg_.device().mailEnabled) {
    if (testPending_)
      lastResult_ = "Mailversand ist ausgeschaltet";
    testPending_ = false;
    return;
  }
  updateLevels();
  const uint32_t ep = hal::epoch();
  if (epochValid(ep) && !epochValid(sched_.startedAt())) {
    sched_.setStartedAt(ep);
    LOGD(Mod::Mail, "Startzeitpunkt nachgetragen");
  }
  Mail::Kind kind = testPending_ ? Mail::Kind::Alive : sched_.due(ep);
  if (kind == Mail::Kind::None) {
    LOGD(Mod::Mail, "nichts fällig: Alarm=%d bereit=%d", sched_.hasAlarm() ? 1 : 0,
         sched_.sensorsReady() ? 1 : 0);
    return;
  }
  sendingTest_ = testPending_;
  testPending_ = false;
  lastAttempt_ = ep;
  sending_ = kind;
  startSend(kind, now); // meldet Fehler selbst über finishSend()
}

size_t MailService::renderSubject(Mail::Kind kind, char* out, size_t cap) {
  Values v;
  collect(v, cfg_, net_, sys_);
  MailVorlage::Umgebung u;
  u.werte = v.paare;
  u.anzahl = 8;
  struct Ctx {
    MailVorlage::Umgebung* u;
    char* out;
    size_t cap;
  } ctx{&u, out, cap};
  out[0] = '\0';
  tpl_.read(
      TemplateStore::subjectOf(TemplateStore::kindOf(kind)),
      [](const char* line, void* c) {
        Ctx& x = *static_cast<Ctx*>(c);
        MailVorlage::expandiereBetreff(line, *x.u, x.out, x.cap);
        return false; // nur die erste Zeile
      },
      &ctx);
  if (!out[0])
    snprintf(out, cap, "Pflanzensensor");
  return strlen(out);
}

void MailService::renderBody(Mail::Kind kind, Sink& out) {
  Values v;
  collect(v, cfg_, net_, sys_);
  BlockCtx bc{&cfg_, &sensors_};
  MailVorlage::Umgebung u;
  u.werte = v.paare;
  u.anzahl = 8;
  u.bloecke = blockLines;
  u.blockContext = &bc;
  out.writeP(FS_("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                 "<meta name=\"viewport\" content=\"width=device-width\"><style>\n"));
  tpl_.read(MailVorlage::Abschnitt::Stil, styleLine, &out);
  out.writeP(FS_("</style></head><body>\n"));
  Expand e{&u, &out};
  tpl_.read(TemplateStore::bodyOf(TemplateStore::kindOf(kind)), expandLine, &e);
  out.writeP(FS_("</body></html>"));
}

bool MailService::startSend(Mail::Kind kind, Instant now) {
  const DeviceConfig& d = cfg_.device();
  if (!net_.connected()) {
    finishSend(false, -1, "Keine WLAN-Verbindung", now);
    return false;
  }
  if (!epochValid(hal::epoch())) {
    finishSend(false, -1, "Keine gültige Uhrzeit", now);
    return false;
  }
  if (!mailbot::plausibleAddress(d.mailTo.c_str())) {
    finishSend(false, 1, "Empfängeradresse fehlt oder ist unbrauchbar", now);
    return false;
  }
  if (d.mailUrl.empty() || d.mailDeviceId.empty() || cfg_.secrets().mailKey.empty()) {
    finishSend(false, 1, "Mailbot-Zugang unvollständig (Adresse, Geräte-ID oder Schlüssel fehlt)",
               now);
    return false;
  }
  hal::TempBuffer buf(kBufferSize + sizeof(Scratch));
  if (!buf) {
    finishSend(false, -1, "Zu wenig Speicher für den Versand", now);
    return false;
  }
  Scratch& sc = *reinterpret_cast<Scratch*>(buf.data() + kBufferSize);
  uint8_t* key = sc.key;
  const char* k = cfg_.secrets().mailKey.c_str();
  if (base64::decode(k, strlen(k), key, sizeof(sc.key)) != static_cast<int>(mailbot::kKeyLen)) {
    finishSend(false, 1, "Geräteschlüssel ungültig (nicht 32 Byte Base64)", now);
    return false;
  }
  char* host = sc.host;
  char* path = sc.path;
  uint16_t port;
  if (!parseUrl(d.mailUrl.c_str(), host, sizeof(sc.host), port, path, sizeof(sc.path))) {
    finishSend(false, 1, "Mailbot-URL ungültig (nur http://)", now);
    return false;
  }

  const uint32_t heapBefore = hal::heap().free;
  char* subject = sc.subject;
  renderSubject(kind, subject, sizeof(sc.subject));

  mailbot::PlaintextBuilder pb(buf.chars(), kBufferSize);
  if (!pb.begin(d.mailTo.c_str(), subject)) {
    finishSend(false, 1, "Betreff zu lang", now);
    return false;
  }
  mailbot::EscapingBufferSink body = pb.bodySink();
  renderBody(kind, body);
  if (body.rawBytes() > mailbot::kMaxBody || body.overflow()) {
    char msg[96];
    snprintf(msg, sizeof(msg), "Mailtext zu lang: %u von %u Bytes",
             static_cast<unsigned>(body.rawBytes()), static_cast<unsigned>(mailbot::kMaxBody));
    finishSend(false, 1, msg, now);
    return false;
  }
  const size_t plainLen = pb.finish(body, d.deviceName.c_str());
  if (!plainLen) {
    finishSend(false, 1, "Mail passt nicht in den Puffer", now);
    return false;
  }

  const uint32_t ts = hal::epoch();
  uint8_t nonce[mailbot::kNonceLen], tag[mailbot::kTagLen];
  hal::fillRandom(nonce, sizeof(nonce));
  char* aad = sc.aad;
  const size_t aadLen = mailbot::buildAad(aad, sizeof(sc.aad), d.mailDeviceId.c_str(), ts);
  if (!hal::aes256gcmSeal(key, nonce, reinterpret_cast<const uint8_t*>(aad), aadLen, buf.data(),
                          plainLen, tag)) {
    finishSend(false, 1, "Verschlüsselung fehlgeschlagen", now);
    return false;
  }
  memset(sc.key, 0, sizeof(sc.key));

  CountingSink count;
  mailbot::writeEnvelope(count, d.mailDeviceId.c_str(), ts, nonce, buf.data(), plainLen, tag);

  if (!tcp_.connect(host, port, kConnectTimeoutMs)) {
    finishSend(false, -1, "Verbindungsaufbau zum Mailbot fehlgeschlagen", now);
    return false;
  }
  tcp_.resetFailed();
  {
    BufferedSink out(tcp_, sc.sink, sizeof(sc.sink));
    char* mac = sc.mac;
    hal::macString(sc.mac);
    char* head = sc.head;
    snprintf(head, sizeof(sc.head),
             "POST %s HTTP/1.1\r\nHost: %s:%u\r\nContent-Type: application/json\r\n"
             "X-Device-Mac: %s\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",
             path, host, port, mac, static_cast<unsigned>(count.count()));
    out.write(head);
    mailbot::writeEnvelope(out, d.mailDeviceId.c_str(), ts, nonce, buf.data(), plainLen, tag);
  }
  if (tcp_.failed()) {
    tcp_.stop();
    finishSend(false, -1, "Senden an den Mailbot fehlgeschlagen", now);
    return false;
  }
  LOGD(Mod::Mail, "Gesendet: Klartext %u B, Umschlag %u B, Heap vorher %lu, jetzt %lu",
       static_cast<unsigned>(plainLen), static_cast<unsigned>(count.count()),
       static_cast<unsigned long>(heapBefore), static_cast<unsigned long>(hal::heap().free));
  sending_ = kind;
  phase_ = Phase::Waiting;
  waitSince_ = now;
  respLen_ = 0;
  resp_[0] = '\0';
  return true;
}

void MailService::pollResponse(Instant now) {
  while (tcp_.available() > 0 && respLen_ + 1 < sizeof(resp_)) {
    const int n =
        tcp_.read(reinterpret_cast<uint8_t*>(resp_ + respLen_), sizeof(resp_) - 1 - respLen_);
    if (n <= 0)
      break;
    respLen_ += static_cast<size_t>(n);
  }
  resp_[respLen_] = '\0';
  const bool closed = !tcp_.connected() && tcp_.available() <= 0;
  const bool full = respLen_ + 1 >= sizeof(resp_);
  const bool timeout = (now - waitSince_).millis() >= static_cast<int32_t>(kResponseTimeoutMs);
  if (!closed && !full && !timeout)
    return;
  tcp_.stop();
  phase_ = Phase::Idle;
  int status = 0;
  if (strncmp(resp_, "HTTP/1.", 7) == 0)
    status = atoi(resp_ + 9);
  if (status == 200) {
    finishSend(true, 200, "Versand erfolgreich", now);
    return;
  }
  if (status <= 0) {
    finishSend(false, -1, timeout ? "Mailbot antwortet nicht" : "Antwort des Mailbots unlesbar",
               now);
    return;
  }
  char msg[120];
  const char* err = strstr(resp_, "\"error\":\"");
  if (err) {
    err += 9;
    const char* end = strchr(err, '"');
    const int len = end ? static_cast<int>(end - err) : 40;
    snprintf(msg, sizeof(msg), "Mailbot lehnte ab (%d): %.*s", status, len > 60 ? 60 : len, err);
  } else {
    snprintf(msg, sizeof(msg), "Mailbot antwortete mit Status %d", status);
  }
  finishSend(false, status, msg, now);
}

void MailService::finishSend(bool ok, int httpStatus, const char* message, Instant now) {
  const uint32_t ep = hal::epoch();
  const Mail::Kind kind = sending_ == Mail::Kind::None ? Mail::Kind::Alive : sending_;
  sending_ = Mail::Kind::None;
  phase_ = Phase::Idle;
  if (ok) {
    sent_++;
    lastResult_ = message;
    LOGI(Mod::Mail, "Mail verschickt");
    if (!sendingTest_) {
      sched_.markSent(kind, ep);
      state_.setMailTimes(sched_.lastWarning(), sched_.lastAlive(), now);
    }
    sendingTest_ = false;
    return;
  }
  const bool transient = httpStatus <= 0;
  if (sendingTest_ && transient && !now.reached(testDeadline_)) {
    testPending_ = true;
    char buf[120];
    snprintf(buf, sizeof(buf), "Wird gleich noch einmal versucht - %s", message);
    lastResult_ = buf;
    LOGD(Mod::Mail, "%s", buf);
    sendingTest_ = false;
    return;
  }
  lastResult_ = message;
  LOGE(Mod::Mail, "Versand fehlgeschlagen: %s", message);
  if (!sendingTest_) {
    // Auch ein Fehlschlag sperrt, sonst Dauerfeuer; Startmeldung wird übersprungen
    if (kind == Mail::Kind::Boot) {
      sched_.skipBootMail();
    } else {
      sched_.markSent(kind, ep);
      state_.setMailTimes(sched_.lastWarning(), sched_.lastAlive(), now);
    }
  }
  sendingTest_ = false;
}

void MailService::writeStatus(JsonWriter& w) const {
  w.beginObject();
  w.kv(FS_("pending"), testPending_ || phase_ == Phase::Waiting);
  w.kv(FS_("sent"), sent_);
  w.kv(FS_("result"), lastResult_.c_str());
  w.kv(FS_("lastAttempt"), lastAttempt_);
  w.kv(FS_("enabled"), cfg_.device().mailEnabled);
  w.kv(FS_("keySet"), !cfg_.secrets().mailKey.empty());
  w.endObject();
}

} // namespace ps
