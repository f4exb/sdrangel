#!/bin/bash
# The checks are evaluated by check(): the variables they use are not seen as used
# shellcheck disable=SC2034
# Retransmission and send delay end to end: sdrangelsrv with a FLEX and a POCSAG modulator in network mode,
# each on a file output, takes messages with copies on its WebAPI (tx action and message center pages) and
# writes them; file inputs play the files to radio paging demodulators. The message center log has the
# records of both: a record per copy sent (R = 0 for FLEX), one received record per message, the copies
# received marked as duplicates.
#
#   radiopaging_retransmit_e2e.sh [BUILD_DIR] [PORT]
#
# Needs the server (ninja sdrangelsrv featuremessagecentersrv modradiopagingsrv demodradiopagingsrv
# outputfileoutputsrv inputfileinputsrv)

BUILD=${1:-/tmp/sdrangel-build}
PORT=${2:-18095}
WORK=$(mktemp -d)
U=http://127.0.0.1:$PORT/sdrangel
FLEXOUT="$WORK/flex.sdriq"
POCSAGOUT="$WORK/pocsag.sdriq"
failed=0

check() {
    if eval "$2"; then echo "PASS: $1"; else echo "FAIL: $1"; failed=$((failed + 1)); fi
}

curl_code() { curl -s -o /dev/null -w "%{http_code}" -X "$1" "$U$2" -H 'Content-Type: application/json' ${3:+-d "$3"}; }
report() { curl -s "$U/featureset/feature/0/report" | tr -d '\n' | sed 's/\\"/"/g'; }
# tx DEVICESET MESSAGE
tx() {
    curl_code POST "/deviceset/$1/channel/0/actions" "{\"channelType\":\"RadioPagingMod\",\"direction\":1,\"RadioPagingModActions\":{\"tx\":$2}}"
}
# action DEVICESET ACTIONS
action() {
    curl_code POST "/deviceset/$1/channel/0/actions" "{\"channelType\":\"RadioPagingMod\",\"direction\":1,\"RadioPagingModActions\":$2}"
}
sent() { curl -s "$U/deviceset/$1/channel/0/report" | tr -d "\n " | grep -o "\"sent\":[0-9]*" | cut -d: -f2; }
# Records of an address in a direction, one per line (a POCSAG address ends with its function letter); the
# log alone, without the transmitters and the pages
records() {
    report | sed 's/"log": "\[/\n/; s/\]",[^"]*"logged"/\n"logged"/' | sed -n 2p | sed 's/},{/}\n{/g' \
        | grep "\"direction\":\"$1\"" | grep "\"address\":\"0*$2[A-D]*\"" | grep -v "\"is_fragment\":true"
}
count() { records "$1" "$2" | grep -c .; }
# Received records of an address marked as a duplicate (dup) or not (none)
countRx() { records rx "$1" | grep -c "\"duplicate\":\"$2\""; }

mkdir -p "$WORK/config"
(cd "$BUILD" && XDG_CONFIG_HOME="$WORK/config" XDG_DATA_HOME="$WORK/data" LD_LIBRARY_PATH="$BUILD/lib" ./sdrangelsrv --scratch -a 127.0.0.1 -p "$PORT" > "$WORK/server.log" 2>&1 &)

for i in $(seq 50); do curl -s "$U" > /dev/null && break; sleep 0.2; done

curl_code POST /featureset/feature '{"featureType":"MessageCenter"}' > /dev/null

# Transmitters: FLEX 1600/2 network collapse 0 (device set 0), POCSAG 1200 network (device set 1)
for ds in 0 1; do
    out=$FLEXOUT
    [ "$ds" = 1 ] && out=$POCSAGOUT
    curl_code POST "/deviceset?direction=1" > /dev/null
    sleep 0.5
    curl_code PUT "/deviceset/$ds/device" '{"hwType":"FileOutput","direction":1}' > /dev/null
    curl_code PATCH "/deviceset/$ds/device/settings" "{\"deviceHwType\":\"FileOutput\",\"direction\":1,\"fileOutputSettings\":{\"fileName\":\"$out\",\"sampleRate\":48000,\"log2Interp\":0}}" > /dev/null
    curl_code POST "/deviceset/$ds/channel" '{"channelType":"RadioPagingMod","direction":1}' > /dev/null
done
curl_code PATCH /deviceset/0/channel/0/settings '{"channelType":"RadioPagingMod","direction":1,"RadioPagingModSettings":{"protocol":1,"mode":1,"flexMode":0,"flexCollapse":0,"title":"tf","deviation":4800}}' > /dev/null
curl_code PATCH /deviceset/1/channel/0/settings '{"channelType":"RadioPagingMod","direction":1,"RadioPagingModSettings":{"protocol":0,"mode":1,"pocsagSpeed":1200,"title":"tp","deviation":4500}}' > /dev/null

# Refused: out of range, a type FLEX does not number, GSC, one-shot
check "FLEX: 16 copies are refused" '[ "$(tx 0 "{\"protocol\":\"flex\",\"address\":\"1100001\",\"type\":\"alphanumeric\",\"retransmit\":16,\"text\":\"x\"}")" = 400 ]'
check "FLEX: copies of a numeric message are refused" '[ "$(tx 0 "{\"protocol\":\"flex\",\"address\":\"1100001\",\"type\":\"numeric\",\"retransmit\":1,\"text\":\"123\"}")" = 400 ]'
check "POCSAG: 11 copies are refused" '[ "$(tx 1 "{\"protocol\":\"pocsag\",\"address\":\"1100001\",\"type\":\"alphanumeric\",\"retransmit\":11,\"text\":\"x\"}")" = 400 ]'
check "FLEX: a group of one pager is refused" '[ "$(tx 0 "{\"protocol\":\"flex\",\"type\":\"alphanumeric\",\"groupMembers\":\"1100010\",\"text\":\"x\"}")" = 400 ]'
check "FLEX: a binary group message is refused" '[ "$(tx 0 "{\"protocol\":\"flex\",\"type\":\"binary\",\"groupMembers\":\"1100010,1100011\",\"text\":\"A5\"}")" = 400 ]'
check "FLEX: a group message with copies is refused" '[ "$(tx 0 "{\"protocol\":\"flex\",\"type\":\"alphanumeric\",\"groupMembers\":\"1100010,1100011\",\"retransmit\":1,\"text\":\"x\"}")" = 400 ]'
check "an interval over 3600 s is refused" '[ "$(tx 1 "{\"protocol\":\"pocsag\",\"address\":\"1100001\",\"type\":\"alphanumeric\",\"retransmit\":1,\"retransmitInterval\":3601,\"text\":\"x\"}")" = 400 ]'
check "a negative send delay is refused" '[ "$(tx 1 "{\"protocol\":\"pocsag\",\"address\":\"1100001\",\"type\":\"alphanumeric\",\"sendDelay\":-1,\"text\":\"x\"}")" = 400 ]'
curl_code PATCH /deviceset/1/channel/0/settings '{"channelType":"RadioPagingMod","direction":1,"RadioPagingModSettings":{"mode":0}}' > /dev/null
check "one-shot: copies are refused" '[ "$(tx 1 "{\"protocol\":\"pocsag\",\"address\":\"1100001\",\"type\":\"alphanumeric\",\"retransmit\":1,\"text\":\"x\"}")" = 400 ]'
check "one-shot: a send delay is refused" '[ "$(tx 1 "{\"protocol\":\"pocsag\",\"address\":\"1100001\",\"type\":\"alphanumeric\",\"sendDelay\":1,\"text\":\"x\"}")" = 400 ]'
curl_code PATCH /deviceset/1/channel/0/settings '{"channelType":"RadioPagingMod","direction":1,"RadioPagingModSettings":{"mode":1}}' > /dev/null

curl_code POST /deviceset/0/device/run > /dev/null
curl_code POST /deviceset/1/device/run > /dev/null
sleep 1

# FLEX: 2 copies 4 s apart (3 frames), after 2 s. POCSAG: 2 copies 3 s apart, after 2 s; a page of the
# message center with one copy
check "FLEX: alphanumeric with 2 copies is accepted" '[ "$(tx 0 "{\"protocol\":\"flex\",\"address\":\"1100002\",\"type\":\"alphanumeric\",\"retransmit\":2,\"retransmitInterval\":4,\"sendDelay\":2,\"text\":\"FLEX COPIES\"}")" = 202 ]'
check "POCSAG: alphanumeric with 2 copies is accepted" '[ "$(tx 1 "{\"protocol\":\"pocsag\",\"address\":\"1100003\",\"type\":\"alphanumeric\",\"retransmit\":2,\"retransmitInterval\":3,\"sendDelay\":2,\"text\":\"POCSAG COPIES\"}")" = 202 ]'
check "FLEX: a group message is accepted" '[ "$(tx 0 "{\"protocol\":\"flex\",\"type\":\"alphanumeric\",\"groupMembers\":\"1100010, 1100011\",\"text\":\"GROUP TEXT\"}")" = 202 ]'

# Codepages: the text of a page is encoded with its codepage, else the one of the channel; a character without a
# code is refused. The receivers decode with theirs
GRUSSE=$(printf 'Gr\303\274\303\237e')
# Ukrainian: pryvit has the i that the KOI-7 Cyrillic of the FLEX Linguist does not have; dobryi den is in it
PRYVIT=$(printf '\320\277\321\200\320\270\320\262\321\226\321\202')
DOBRYI=$(printf '\320\224\320\276\320\261\321\200\320\270\320\271 \320\264\320\265\320\275\321\214')
# Codepages: vendor, model and codepage in the protocol of the field; the records have the keys sorted
GERMAN='{"vendor":"generic","model":"generic","codepage":"latin-german"}'
CYRILLIC='{"vendor":"motorola","model":"linguist","codepage":"cyrillic"}'
GERMAN_REC='"charset":{"codepage":"latin-german","model":"generic","vendor":"generic"}'
CYRILLIC_REC='"charset":{"codepage":"cyrillic","model":"linguist","vendor":"motorola"}'
check "POCSAG: a character without a code in the codepage is refused" '[ "$(tx 1 "{\"protocol\":\"pocsag\",\"address\":\"1100005\",\"type\":\"alphanumeric\",\"text\":\"$GRUSSE\"}")" = 400 ]'
check "POCSAG: a FLEX codepage is refused" '[ "$(tx 1 "{\"protocol\":\"pocsag\",\"address\":\"1100005\",\"type\":\"alphanumeric\",\"charset\":$CYRILLIC,\"text\":\"x\"}")" = 400 ]'
check "POCSAG: German text with the German codepage is accepted" '[ "$(tx 1 "{\"protocol\":\"pocsag\",\"address\":\"1100005\",\"type\":\"alphanumeric\",\"charset\":$GERMAN,\"text\":\"$GRUSSE\"}")" = 202 ]'
check "the codepage setting of a transmitter must be one of the protocol" '[ "$(curl_code PATCH /deviceset/0/channel/0/settings "{\"channelType\":\"RadioPagingMod\",\"direction\":1,\"RadioPagingModSettings\":{\"flexCharset\":$GERMAN}}")" = 400 ]'
curl_code PATCH /deviceset/0/channel/0/settings "{\"channelType\":\"RadioPagingMod\",\"direction\":1,\"RadioPagingModSettings\":{\"flexCharset\":$CYRILLIC}}" > /dev/null
check "FLEX: a Ukrainian i without a code in the codepage of the channel is refused" '[ "$(tx 0 "{\"protocol\":\"flex\",\"address\":\"1100006\",\"type\":\"alphanumeric\",\"text\":\"$PRYVIT\"}")" = 400 ]'
check "FLEX: Ukrainian text with the codepage of the channel is accepted" '[ "$(tx 0 "{\"protocol\":\"flex\",\"address\":\"1100006\",\"type\":\"alphanumeric\",\"text\":\"$DOBRYI\"}")" = 202 ]'
# BeepwearPRO alarm: a codepage with a special format, the text key=value
BEEPWEAR='{"vendor":"timex","model":"beepwear-pro","codepage":"alarm"}'
check "FLEX: a BeepwearPRO alarm without its minute is refused" '[ "$(tx 0 "{\"protocol\":\"flex\",\"address\":\"1100007\",\"type\":\"alphanumeric\",\"charset\":$BEEPWEAR,\"text\":\"hour=7 msg=Wake up\"}")" = 400 ]'
check "FLEX: a BeepwearPRO alarm is accepted" '[ "$(tx 0 "{\"protocol\":\"flex\",\"address\":\"1100007\",\"type\":\"alphanumeric\",\"charset\":$BEEPWEAR,\"text\":\"hour=7 min=0 msg=Wake up\"}")" = 202 ]'
# Japanese Text Service: the text is sent as a HEX/binary message of 16-bit Shift JIS characters
KANJI='{"vendor":"generic","model":"generic","codepage":"kanji"}'
NIHON=$(printf '\346\227\245\346\234\254')
check "FLEX: Ukrainian text is refused with Kanji" '[ "$(tx 0 "{\"protocol\":\"flex\",\"address\":\"1100008\",\"type\":\"alphanumeric\",\"charset\":$KANJI,\"text\":\"$PRYVIT\"}")" = 400 ]'
check "FLEX: Japanese text with Kanji is accepted" '[ "$(tx 0 "{\"protocol\":\"flex\",\"address\":\"1100008\",\"type\":\"alphanumeric\",\"charset\":$KANJI,\"text\":\"$NIHON\"}")" = 202 ]'

# The page waits for the modulator to announce itself (every 5 s)
check "a page with a copy is accepted by the message center" '[ "$(curl_code POST /featureset/feature/0/actions "{\"featureType\":\"MessageCenter\",\"MessageCenterActions\":{\"post\":{\"id\":\"p-rep\",\"transmitter\":\"tp\",\"protocol\":\"pocsag\",\"address\":\"1100004\",\"type\":\"alphanumeric\",\"retransmit\":1,\"retransmitInterval\":2,\"text\":\"PAGE COPY\"}}}")" = 202 ]'

# FLEX System Event Notification: refused out of range and on POCSAG, then sent in each frame
check "System Event Notification: flags over 2047 are refused" '[ "$(action 0 "{\"systemEvent\":2048}")" = 400 ]'
check "System Event Notification: 100 frames are refused" '[ "$(action 0 "{\"systemEvent\":5,\"systemEventFrames\":100}")" = 400 ]'
check "System Event Notification: refused on POCSAG" '[ "$(action 1 "{\"systemEvent\":5}")" = 400 ]'
check "System Event Notification: accepted on the FLEX network" '[ "$(action 0 "{\"systemEvent\":5}")" = 202 ]'

for i in $(seq 60); do [ "$(count tx 1100002)" = 3 ] && [ "$(count tx 1100003)" = 3 ] && [ "$(count tx 1100004)" = 2 ] && break; sleep 1; done
check "FLEX: a record for the message and each copy" '[ "$(count tx 1100002)" = 3 ]'
check "POCSAG: a record for the message and each copy" '[ "$(count tx 1100003)" = 3 ]'
check "page: a record for the page and its copy" '[ "$(count tx 1100004)" = 2 ]'
check "FLEX: the copies have R = 0 and their number" '[[ "$(records tx 1100002 | grep "\"retransmission\":1" | grep -c "\"retrieval\":false")" = 1 && "$(records tx 1100002 | grep "\"retransmission\":2" | grep -c "\"retrieval\":false")" = 1 ]]'
check "page: sent once, with its first transmission" '[[ "$(report | sed "s/},{/}\n{/g" | grep "\"id\":\"p-rep\"" | grep -c "\"state\":\"sent\"")" -ge 1 ]]'
check "the modulators report each message once" '[ "$(sent 0)" = 5 ] && [ "$(sent 1)" = 3 ]'
check "FLEX: the Kanji record sent is HEX/binary of B = 16, with the text and its codepage" '[[ "$(records tx 1100008)" == *"\"text\":\"$NIHON\""* && "$(records tx 1100008)" == *"\"msgType\":\"binary\""* && "$(records tx 1100008)" == *"\"blocking\":16"* && "$(records tx 1100008)" == *"\"hex\":\"AF39B769\""* && "$(records tx 1100008)" == *"\"codepage\":\"kanji\""* ]]'
check "FLEX: the BeepwearPRO alarm record has the key=value text and its codepage" '[[ "$(records tx 1100007)" == *"\"text\":\"hour=7 min=0 msg=Wake up\""* && "$(records tx 1100007)" == *"\"model\":\"beepwear-pro\""* ]]'
check "FLEX: a record for the group message, with its pagers" '[[ "$(report | sed "s/},{/}\n{/g" | grep "\"direction\":\"tx\"" | grep "GROUP TEXT" | grep -c "group_members")" = 1 ]]'
check "POCSAG: the record sent has the text, its codepage and the message bits" '[[ "$(records tx 1100005)" == *"$GRUSSE"* && "$(records tx 1100005)" == *"$GERMAN_REC"* && "$(records tx 1100005)" == *"\"rawBits\":40"* ]]'
check "FLEX: the record sent has the codepage of the channel" '[[ "$(records tx 1100006)" == *"$DOBRYI"* && "$(records tx 1100006)" == *"$CYRILLIC_REC"* ]]'
sleep 2
curl_code DELETE /deviceset/0/device/run > /dev/null
curl_code DELETE /deviceset/1/device/run > /dev/null
sleep 1

# Receivers: the files played to demodulators; the copies are duplicates of the messages
for ds in 2 3; do
    in=$FLEXOUT
    [ "$ds" = 3 ] && in=$POCSAGOUT
    curl_code POST "/deviceset?direction=0" > /dev/null
    sleep 0.5
    curl_code PUT "/deviceset/$ds/device" '{"hwType":"FileInput","direction":0}' > /dev/null
    curl_code PATCH "/deviceset/$ds/device/settings" "{\"deviceHwType\":\"FileInput\",\"direction\":0,\"fileInputSettings\":{\"fileName\":\"$in\",\"accelerationFactor\":1,\"loop\":0}}" > /dev/null
    curl_code POST "/deviceset/$ds/channel" '{"channelType":"RadioPagingDemod","direction":0}' > /dev/null
done
curl_code PATCH /deviceset/2/channel/0/settings '{"channelType":"RadioPagingDemod","direction":0,"RadioPagingDemodSettings":{"pocsagEnabled":0,"flexEnabled":1,"gscEnabled":0,"flexDeviation":4800,"title":"rf","flexCharset":{"vendor":"motorola","model":"linguist","codepage":"cyrillic"}}}' > /dev/null
curl_code PATCH /deviceset/3/channel/0/settings '{"channelType":"RadioPagingDemod","direction":0,"RadioPagingDemodSettings":{"pocsagEnabled":1,"flexEnabled":0,"gscEnabled":0,"pocsagDeviation":4500,"title":"rp","pocsagCharset":{"vendor":"generic","model":"generic","codepage":"latin-german"}}}' > /dev/null
check "the codepage setting of a receiver must be one of the protocol" '[ "$(curl_code PATCH /deviceset/3/channel/0/settings "{\"channelType\":\"RadioPagingDemod\",\"direction\":0,\"RadioPagingDemodSettings\":{\"gscCharset\":$CYRILLIC}}")" = 400 ]'
check "the POCSAG fill codes must be hex codes 00 to 7F" '[ "$(curl_code PATCH /deviceset/3/channel/0/settings "{\"channelType\":\"RadioPagingDemod\",\"direction\":0,\"RadioPagingDemodSettings\":{\"pocsagFillCodes\":\"00 80\"}}")" = 400 ]'
curl_code PATCH /deviceset/3/channel/0/settings '{"channelType":"RadioPagingDemod","direction":0,"RadioPagingDemodSettings":{"pocsagFill":1}}' > /dev/null
curl_code POST /deviceset/2/device/run > /dev/null
curl_code POST /deviceset/3/device/run > /dev/null

SECS=$(( $(stat -c %s "$FLEXOUT") / (48000 * 4) + 5 ))
sleep "$SECS"

check "FLEX: received once, the copies marked as duplicates" '[ "$(countRx 1100002 none)" = 1 ] && [ "$(countRx 1100002 dup)" = 2 ]'
check "FLEX: the message received has R = 1, the copies R = 0" '[[ "$(records rx 1100002 | grep "\"duplicate\":\"none\"")" == *"\"retrieval\":true"* && "$(records rx 1100002 | grep "\"duplicate\":\"dup\"" | grep -c "\"retrieval\":false")" = 2 ]]'
check "FLEX: the copies have the number of the message" '[ "$(records rx 1100002 | grep -c "\"message_number\":0")" = 3 ]'
check "POCSAG: received once, the copies marked as duplicates" '[ "$(countRx 1100003 none)" = 1 ] && [ "$(countRx 1100003 dup)" = 2 ]'
check "POCSAG page: received once, its copy marked as a duplicate" '[ "$(countRx 1100004 none)" = 1 ] && [ "$(countRx 1100004 dup)" = 1 ]'
check "FLEX: the group message is received on its temporary address with its pagers" '[[ "$(records rx 2029568 | grep "\"msgType\":\"alphanumeric\"" | grep -c "group_capcodes")" -ge 1 ]]'
check "FLEX: both pagers have their short instruction" '[ "$(report | sed "s/},{/}\n{/g" | grep "\"direction\":\"rx\"" | grep -c "short_instruction")" -ge 2 ]'
check "FLEX: the System Event Notification is received in frames, its flags" '[ "$(report | sed "s/},{/}\n{/g" | grep "\"direction\":\"rx\"" | grep -c "\"event_flags\":5")" -ge 3 ]'
rawData() { records "$1" "$2" | head -1 | grep -o "\"rawData\":\"[^\"]*\""; }
check "POCSAG: received in the German codepage, the bits as sent" '[[ "$(records rx 1100005)" == *"$GRUSSE"* && -n "$(rawData tx 1100005)" && "$(rawData tx 1100005)" == "$(rawData rx 1100005)" ]]'
check "FLEX: received in the Cyrillic codepage" '[[ "$(records rx 1100006)" == *"$DOBRYI"* && "$(records rx 1100006)" == *"$CYRILLIC_REC"* ]]'
check "the received records keep what decodes them again: POCSAG fill codes, FLEX characters" '[[ "$(records rx 1100005)" == *"\"fill_codes\":[0,3,4]"* && "$(records rx 1100006)" == *"\"alpha_codes\":\""* ]]'
check "FLEX: the Kanji message is received as HEX/binary of B = 16 with the Cyrillic codepage" '[[ "$(records rx 1100008)" == *"\"text\":\"AF39B769\""* && "$(records rx 1100008)" == *"\"blocking\":16"* && "$(records rx 1100008)" == *"\"hex\":\"AF39B769\""* ]]'
# Decoding again (message center WebAPI): the record of the log is replaced
redecode() { curl_code POST /featureset/feature/0/actions "{\"featureType\":\"MessageCenter\",\"MessageCenterActions\":{\"redecode\":$1}}"; }
RXID=$(records rx 1100005 | head -1 | grep -o "\"id\":\"[^\"]*\"" | head -1 | cut -d\" -f4)
ASCII='{"vendor":"generic","model":"generic","codepage":"ascii"}'
check "decode again: an unknown record is not found" '[ "$(redecode "{\"id\":\"none\"}")" = 404 ]'
check "decode again: a codepage of another protocol is refused" '[ "$(redecode "{\"id\":\"$RXID\",\"charset\":$CYRILLIC}")" = 400 ]'
check "decode again: POCSAG with the ASCII codepage" '[ "$(redecode "{\"id\":\"$RXID\",\"charset\":$ASCII}")" = 202 ]'
sleep 1
check "decode again: the record has the ASCII text and codepage" '[[ "$(records rx 1100005 | grep "\"id\":\"$RXID\"")" == *"\"text\":\"Gr}~e\""* && "$(records rx 1100005 | grep "\"id\":\"$RXID\"")" == *"\"redecoded\":true"* ]]'
check "decode again: POCSAG as numeric" '[ "$(redecode "{\"id\":\"$RXID\",\"type\":\"numeric\"}")" = 202 ]'
sleep 1
check "decode again: the record is numeric, alphanumeric the other candidate" '[[ "$(records rx 1100005 | grep "\"id\":\"$RXID\"")" == *"\"msgType\":\"numeric\""* && "$(records rx 1100005 | grep "\"id\":\"$RXID\"")" == *"\"alt_type\":\"alphanumeric\""* ]]'
KANJIID=$(records rx 1100008 | head -1 | grep -o "\"id\":\"[^\"]*\"" | head -1 | cut -d\" -f4)
check "decode again: FLEX HEX/binary with Kanji" '[ "$(redecode "{\"id\":\"$KANJIID\",\"charset\":$KANJI}")" = 202 ]'
sleep 1
check "decode again: the HEX/binary record has the Japanese text" '[[ "$(records rx 1100008 | grep "\"id\":\"$KANJIID\"")" == *"\"text\":\"$NIHON\""* && "$(records rx 1100008 | grep "\"id\":\"$KANJIID\"")" == *"\"codepage\":\"kanji\""* ]]'
# How the FLEX messages went is reported when they are in their frame: message number, R, fragments
check "FLEX: the records sent have the message number, R and the fragments" '[[ "$(records tx 1100002 | grep -c "\"message_number\":0")" = 3 && "$(records tx 1100002 | grep -v "retransmission" | grep "\"retrieval\":true" | grep -c "\"fragment_count\":1")" = 1 ]]'

if [ "$failed" -ne 0 ]; then
    report | sed 's/},{/}\n{/g' | grep "11000" | sed 's/"ber".*"direction"/"direction"/; s/"errorsCorrected".*"id"/"id"/; s/"rawBits".*"text"/"text"/'
fi

curl_code DELETE /deviceset/2/device/run > /dev/null
curl_code DELETE /deviceset/3/device/run > /dev/null
fuser -k "$PORT"/tcp > /dev/null 2>&1
rm -rf "$WORK"
echo "$failed failed"
exit $failed
