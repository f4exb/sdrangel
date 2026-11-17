#!/bin/bash
# The checks are evaluated by check(): the variables they use are not seen as used
# shellcheck disable=SC2034
# Message center end to end: sdrangelsrv with the message center feature and a GSC modulator on a
# file output. The modulator announces itself, pages are posted to the message center (WebAPI
# action), claimed and transmitted by the modulator; the states are read back from the report.
#
#   messagecenter_e2e.sh [BUILD_DIR] [PORT]
#
# Needs the server (ninja sdrangelsrv featuremessagecentersrv modradiopagingsrv outputfileoutputsrv)

BUILD=${1:-/tmp/sdrangel-build}
PORT=${2:-18093}
WORK=$(mktemp -d)
U=http://127.0.0.1:$PORT/sdrangel
OUT="$WORK/out.sdriq"
failed=0

check() {
    if eval "$2"; then echo "PASS: $1"; else echo "FAIL: $1"; failed=$((failed + 1)); fi
}

curl_code() { curl -s -o /dev/null -w "%{http_code}" -X "$1" "$U$2" -H 'Content-Type: application/json' ${3:+-d "$3"}; }
report() { curl -s "$U/featureset/feature/0/report" | tr -d '\n' | sed 's/\\"/"/g'; }
post() {
    curl_code POST /featureset/feature/0/actions "{\"featureType\":\"MessageCenter\",\"MessageCenterActions\":{\"post\":$1}}"
}

mkdir -p "$WORK/config"
(cd "$BUILD" && XDG_CONFIG_HOME="$WORK/config" XDG_DATA_HOME="$WORK/data" LD_LIBRARY_PATH="$BUILD/lib" ./sdrangelsrv --scratch -a 127.0.0.1 -p "$PORT" > "$WORK/server.log" 2>&1 &)

# Up to a minute: the server probes the devices first (a USRP takes a while)
for i in $(seq 300); do curl -s "$U" > /dev/null && break; sleep 0.2; done

curl_code POST /featureset/feature '{"featureType":"MessageCenter"}' > /dev/null
curl_code POST "/deviceset?direction=1" > /dev/null
sleep 0.5
curl_code PUT /deviceset/0/device '{"hwType":"FileOutput","direction":1}' > /dev/null
curl_code PATCH /deviceset/0/device/settings "{\"deviceHwType\":\"FileOutput\",\"direction\":1,\"fileOutputSettings\":{\"fileName\":\"$OUT\",\"sampleRate\":48000,\"log2Interp\":0}}" > /dev/null
curl_code POST /deviceset/0/channel '{"channelType":"RadioPagingMod","direction":1}' > /dev/null
curl_code PATCH /deviceset/0/channel/0/settings '{"channelType":"RadioPagingMod","direction":1,"RadioPagingModSettings":{"protocol":2,"mode":0,"title":"t1","deviation":4500}}' > /dev/null
curl_code POST /deviceset/0/device/run > /dev/null

# The modulator announces itself every 5 s
sleep 7
R=$(report)
check "the modulator is registered under its name (title), idle" '[[ "$R" == *"\"channel\":\"t1\""*"\"role\":\"tx\""*"\"status\":\"idle\""* ]]'

check "a page is accepted" '[ "$(post "{\"id\":\"p-tone\",\"transmitter\":\"t1\",\"protocol\":\"gsc\",\"address\":\"123456\",\"function\":1,\"type\":\"tone\"}")" = 202 ]'
check "a page with text is accepted" '[ "$(post "{\"id\":\"p-alpha\",\"transmitter\":\"t1\",\"protocol\":\"gsc\",\"address\":\"123456\",\"function\":2,\"type\":\"alphanumeric\",\"text\":\"FROM THE CENTER\"}")" = 202 ]'
check "a page for a transmitter that is not there is accepted" '[ "$(post "{\"id\":\"p-nobody\",\"transmitter\":\"nobody\",\"protocol\":\"gsc\",\"address\":\"123456\",\"function\":0,\"type\":\"tone\"}")" = 202 ]'
check "a page with a bad address is refused" '[ "$(post "{\"id\":\"p-bad\",\"transmitter\":\"t1\",\"protocol\":\"flex\",\"address\":\"abc\"}")" = 400 ]'
check "a voice page needs a file" '[ "$(post "{\"id\":\"p-voice\",\"transmitter\":\"t1\",\"protocol\":\"gsc\",\"address\":\"123456\",\"type\":\"voice\"}")" = 400 ]'

# A contact: its rule names the messages of its address, its send to gives the transmitter and the address of a page to it
CONTACTS='[{\"name\":\"Duty\",\"icon\":\":pager\",\"rules\":[[{\"field\":\"protocol\",\"is\":\"gsc\"},{\"field\":\"address\",\"matches\":\"123456/.\"}]],\"sendTo\":[{\"page\":{\"transmitter\":\"t1\",\"protocol\":\"gsc\",\"address\":\"123456\",\"function\":3}}]}]'
check "the contacts are set" '[ "$(curl_code PATCH /featureset/feature/0/settings "{\"featureType\":\"MessageCenter\",\"MessageCenterSettings\":{\"contacts\":\"$CONTACTS\"}}")" = 200 ]'
check "a contact with an address that is not one is refused" '[ "$(curl_code PATCH /featureset/feature/0/settings "{\"featureType\":\"MessageCenter\",\"MessageCenterSettings\":{\"contacts\":\"[{\\\"name\\\":\\\"X\\\",\\\"sendTo\\\":[{\\\"page\\\":{\\\"transmitter\\\":\\\"t1\\\",\\\"protocol\\\":\\\"gsc\\\",\\\"address\\\":\\\"abc\\\"}}]}]\"}}")" = 400 ]'
check "a page to a contact is accepted" '[ "$(post "{\"id\":\"p-contact\",\"contact\":\"Duty\",\"type\":\"alphanumeric\",\"text\":\"TO A CONTACT\"}")" = 202 ]'
check "a page to a contact that is not there is refused" '[ "$(post "{\"id\":\"p-nocontact\",\"contact\":\"Nobody\",\"type\":\"tone\"}")" = 400 ]'

sleep 12
R=$(report)
check "the page to the contact was sent by its transmitter to its address, the log names it" '[[ "$R" == *"\"id\":\"p-contact\""*"\"state\":\"sent\""* && "$R" == *"\"contact\":\"Duty\""* && "$R" == *"123456/4"* ]]'
check "the page was claimed by the modulator and sent" '[[ "$R" == *"\"claimedBy\":\"t1\""*"\"id\":\"p-tone\""*"\"state\":\"sent\""* ]]'
check "the page with text was sent" '[[ "$R" == *"\"id\":\"p-alpha\""*"\"state\":\"sent\""*"\"text\":\"FROM THE CENTER\""* ]]'
check "the page for nobody waits" '[[ "$R" == *"\"id\":\"p-nobody\""*"\"state\":\"pending\""* ]]'
check "the sent messages are in the log, from the transmitter" '[[ "$R" == *"\"channel\":\"t1\""*"\"direction\":\"tx\""* ]]'
check "the modulator sent three messages" '[ "$(curl -s "$U/deviceset/0/channel/0/report" | tr -d "\n " | grep -o "\"sent\":[0-9]*")" = "\"sent\":3" ]'

check "a page nobody has can be withdrawn" '[ "$(curl_code POST /featureset/feature/0/actions "{\"featureType\":\"MessageCenter\",\"MessageCenterActions\":{\"cancel\":\"p-nobody\"}}")" = 202 ]'
sleep 1.5
R=$(report)
# (the pages have a state, the records of the log do not)
check "the withdrawn page is gone" '! echo "$R" | grep -q "\"id\":\"p-nobody\",\"protocol\"[^}]*\"state\""'

# Ingest: a record needs its protocol; its time and id are given when it has none. Routed (the default route logs it)
check "an ingested record without id and timestamp is accepted" '[ "$(curl_code POST /featureset/feature/0/actions '"'"'{"featureType":"MessageCenter","MessageCenterActions":{"ingest":"{\"protocol\":\"pocsag\",\"address\":\"0007777A\",\"msgType\":\"alphanumeric\",\"text\":\"INGESTED TEXT\"}"}}'"'"')" = 202 ]'
check "an ingested record without protocol is refused" '[ "$(curl_code POST /featureset/feature/0/actions '"'"'{"featureType":"MessageCenter","MessageCenterActions":{"ingest":"{\"address\":\"0007777A\",\"text\":\"x\"}"}}'"'"')" = 400 ]'
sleep 1.5
check "the ingested record is in the log, with an id and a time" '[[ "$(report)" == *"\"id\":\"api-"*"\"text\":\"INGESTED TEXT\""* || "$(report)" == *"\"text\":\"INGESTED TEXT\""*"\"id\":\"api-"* ]]'

check "the settings are read" '[[ "$(curl -s "$U/featureset/feature/0/settings" | tr -d "\n")" == *"\"logSize\": 10000"* ]]'
check "a route that is not JSON is refused" '[ "$(curl_code PATCH /featureset/feature/0/settings "{\"featureType\":\"MessageCenter\",\"MessageCenterSettings\":{\"routes\":\"not json\"}}")" = 400 ]'
ROUTES='[{\"name\":\"gsc to the file\",\"enabled\":true,\"stop\":false,\"sinks\":[\"log\"],\"filter\":{\"protocols\":[\"gsc\"]}}]'
check "routes and the claim time are set" '[ "$(curl_code PATCH /featureset/feature/0/settings "{\"featureType\":\"MessageCenter\",\"MessageCenterSettings\":{\"claimTimeoutSec\":20,\"routes\":\"$ROUTES\"}}")" = 200 ]'
sleep 1
S=$(curl -s "$U/featureset/feature/0/settings" | tr -d "\n" | sed 's/\\"/"/g')
check "the settings keep them" '[[ "$S" == *"\"claimTimeoutSec\": 20"* && "$S" == *"gsc to the file"* ]]'
check "the reverse API is off, its defaults are there" '[[ "$S" == *"\"useReverseAPI\": 0"* && "$S" == *"\"reverseAPIAddress\": \"127.0.0.1\""* && "$S" == *"\"reverseAPIPort\": 8888"* ]]'
check "the reverse API settings are set" '[ "$(curl_code PATCH /featureset/feature/0/settings "{\"featureType\":\"MessageCenter\",\"MessageCenterSettings\":{\"reverseAPIAddress\":\"127.0.0.2\",\"reverseAPIPort\":8899,\"reverseAPIFeatureSetIndex\":1,\"reverseAPIFeatureIndex\":2}}")" = 200 ]'
sleep 1
S=$(curl -s "$U/featureset/feature/0/settings" | tr -d "\n" | sed 's/\\"/"/g')
check "the settings keep the reverse API" '[[ "$S" == *"\"reverseAPIAddress\": \"127.0.0.2\""* && "$S" == *"\"reverseAPIPort\": 8899"* && "$S" == *"\"reverseAPIFeatureSetIndex\": 1"* && "$S" == *"\"reverseAPIFeatureIndex\": 2"* ]]'

# History: the log is in its file and is there again in a new message center
check "the history is on by default, 500 MB, 30 days" '[[ "$S" == *"\"historyEnabled\": 1"* && "$S" == *"\"historyMaxMB\": 500"* && "$S" == *"\"historyDays\": 30"* ]]'
check "the history has the log, in a file of the day" '[ -n "$(find "$WORK/data" -path "*messagecenter/history/*" -name "$(date +%Y%m%d)-000.jsonl" -size +0 2>/dev/null)" ]'

# A query of the history: the sent pages with text of the last hour, then a page of one
QUERY='{\"last\":3600,\"filter\":{\"direction\":1,\"text\":\"center|contact\"},\"limit\":1}'
check "a query is accepted" '[ "$(curl_code POST /featureset/feature/0/actions "{\"featureType\":\"MessageCenter\",\"MessageCenterActions\":{\"query\":\"$QUERY\"}}")" = 202 ]'
Q=$(report)
QR=${Q#*\"query\": \"}
check "its result in the report: the total of the messages it selects, a page of one, the query echoed" '[[ "$QR" == "{\"query\":{"*"\"limit\":1"*"\"total\":2"* && $(grep -o "\"direction\":\"tx\"" <<< "$QR" | wc -l) -eq 1 ]]'
check "a query with a filter that is not valid is refused" '[ "$(curl_code POST /featureset/feature/0/actions "{\"featureType\":\"MessageCenter\",\"MessageCenterActions\":{\"query\":\"{\\\"filter\\\":{\\\"text\\\":\\\"([\\\"}}\"}}")" = 400 ]'
curl_code DELETE /featureset/feature/0 > /dev/null
sleep 1
curl_code POST /featureset/feature '{"featureType":"MessageCenter"}' > /dev/null
sleep 2
check "a new message center has the log again" '[[ "$(report)" == *"\"text\":\"FROM THE CENTER\""* ]]'

curl_code DELETE /deviceset/0/device/run > /dev/null
fuser -k "$PORT"/tcp > /dev/null 2>&1
rm -rf "$WORK"
echo "$failed failed"
exit $failed
