<h1>Message Center feature plugin</h1>

<h2>Introduction</h2>

The Message Center collects the messages of the radio paging channels (Radio Paging demodulator and modulator) in one log, routes them to files, and queues pages to the transmitters. It is the place where messages, events and voice messages are exchanged between the decoders and the encoders.

The channels publish their messages as message records on the "messages" pipe, and announce themselves with event records: the Message Center knows the receivers and transmitters without configuration. It is added like any feature, or with the Message Center button of the Radio Paging demodulator and modulator, which opens it (and adds it when there is none).

```
  source ---> rules 1, 2, 3... (when, change) ---> sink(s)
  sources: the channels, the API, a JSON lines file, a CSV log, UDP datagrams
  sinks:   the log, the transmit queue, a JSON lines file, a CSV file, UDP datagrams,
           a command, speech, a mark in the log, a position on the Map
```

<h2>Interface</h2>

![Message Center plugin GUI: the messages received over the air from a Radio Paging modulator, POCSAG at 512, 1200 and 2400 baud (one inverted), FLEX 1600/2, 3200/4 and 6400/4 and GSC, with their contacts and the marks of the Emergency preset](../../../doc/img/MessageCenter_plugin.png)

The window has five tabs on its left side: Messages (the log, live or searched), Send (a page, and the queue of the pages), Contacts, Rules and Connections (the channels, the sinks and the sources). Options... at the bottom of Messages opens the options.

<h3>Log</h3>

The messages received and sent, newest at the bottom.

* Live: the newest 100 messages the filters select, as they come; Live: 100 of 1234 matching (3777 in the log) says how many of the log (in memory) the filters select. While a row is selected or the table is scrolled up, the rows stay where they are: the new messages wait, and a button in the corner of the table counts them; they come when it is clicked, or when the table is at the bottom again with no row selected.
* Query: the messages of a time range the filters select, over the history (the log when the history is not kept): the last 5 minutes, hour, day, 7 days, all, or from a time to a time. The range is fixed when the query is run (Run, a filter changed), so the pages keep it: how many messages it selects, and a page of 100 of them, newest first (Newer, Older). It stays as it is until it is run again; the auto-refresh runs it again every 5 to 60 s (none by default; not while a row is selected).

![Message Center log, a query: the messages received of the last day, 21 of them](../../../doc/img/MessageCenter_plugin_query.png)

* Filters: contact, protocol, direction (RX, TX or both), polarity, message type (the types of the protocol chosen), quality, channel name, address, text, conditions, hide duplicates, voice pages with a file only. All the filters set must match.
  * Quality: how well the message came through, as its receiver judges it. Clean: no bit errors and every check passed (FLEX checksum K and signature S, GSC blocks). Corrected: bit errors, all corrected, and the checks passed (or there are none: POCSAG). Damaged: something lost: uncorrectable errors, an incomplete message, or a failed check, which says the corrections were wrong. The checksum K is of each fragment (a reassembled message has the worst of its fragments), the signature S of the whole message. Not damaged: clean or corrected, or nothing to check (a tone). A message sent is clean.
  * Address: a regular expression the whole address must match, as the filters of the other plugins of SDRangel: 12345.. for the addresses 1234500 to 1234599.
  * Text: a regular expression found anywhere in the text, not case sensitive: fire, or fire|smoke (^ and $ for its start and end).
  * Conditions...: conditions on any field of the message or of its protocol (the details of a message show them all: capcode, ric, function, checksum, signature, blocking, group_capcodes...), all of them met. A field is chosen in the list, by group: the common fields, the fields of each protocol as the channels describe them (their name, and the values to choose from: checksum ok or failed, phase A to D...), and the other fields of the messages of the log; or its key is typed; the operator is is, is not (another value, or none), matches (a regular expression of the whole value: capcode 4071410.. for a range of capcodes), <, <=, >, >= (numbers), is present or is absent. E.g. checksum is failed. A list (the members of a group message): one of its values.
  * Fragments (above the table): the fragments of a FLEX message as rows of their own; hidden by default, the whole message is one row.

  An expression that is not valid is shown in red and selects nothing. Only the messages the filters select are shown. An address selects the FLEX group messages of which it is a member too (the members received with the assignments).
* The table has the columns of the message tables of the channels: date, time, direction, channel, protocol, rate, polarity, contact, address, type, message, fragment, info, quality and state. Right click on the header selects the columns. The direction is RX (received) or TX (sent by its channel); a page posted to the Message Center that is not sent yet is QU while it is in the queue and NS (not sent) when it failed, was cancelled or is no longer queued (the queue is not kept when SDRangel is restarted). The tooltip of QU and NS gives the state and the reason, e.g. "waiting: tx1 sends pocsag now, not flex".
* A click on the icon of a voice message plays its file. Double click on a message opens a window with all the fields of its record: the common fields, protocolData (the flags and parameters of the protocol) and the raw payload, also as bits.
* Right click on a message: details, copy the cell, the row or the record as JSON, play the voice, select the audio output, and decode a received message again as the other type or with another codepage of its protocol.
* Decode as, All codepages...: a window with the message decoded with every codepage of its protocol (and as numeric, POCSAG and GSC), a row each; the reading shown is in bold. Right click on a row: Create rule..., the rule dialog filled to decode the messages of the address so.
* Create rule... (right click on a message): the rule dialog filled from the message: its protocol, polarity, type and address, to the log, with Stop. The tables of the Radio Paging channels have both too, for the Message Center there is (none: they are greyed out).
* The filters are on a line that wraps at the width of the window.
* Import (open icon): reads the messages of a file into the log: a JSON lines file, or a CSV log of a message center or of the Pager demodulator. They are logged, not routed, so no action is taken on them again.
* Export (save icon): saves the messages the filters select to a CSV file, which Import reads again.
* Clear (bin icon): removes all the messages of the log (in memory) after a confirmation; the history stays, unless "Also delete the history" is checked there. The items the map sinks put on the Map are removed too.
* A message a rule marked has the icon of the mark before its text, its text and its row in the colours of the mark (those it sets). A message several rules mark has their icons side by side. The marks are in the record (protocolData marks): the history, the exports and the WebAPI have them.

<h3>Channels</h3>

The channels that announced themselves: name (the title of the channel), role (receiver or transmitter), status (online, or offline when it has not announced itself for the silent time), protocols, speeds, polarity, the pages queued to it, and the time it was last seen.

* A channel renamed (its title changed) keeps one entry, of its new name.
* Remove: removes the channel selected; it comes back when it announces itself again. Remove offline: removes the channels offline. The pages of a transmitter removed go back to the queue.

<h3>Queue</h3>

The pages to transmit, with the transmitter (its name), protocol, address, text, state, tries, retries and the reason of a failure or of the wait of a pending page (for example "waiting: tx1 sends pocsag now, not flex": the page goes out once the modulator is switched to FLEX). A page goes through pending, queued, claimed, transmitted and sent: the transmitter claims it and reports its state back. A page given to a transmitter that does not claim it within the claim timeout goes back to the queue; a page that can not be sent is dead.

* Requeue: a dead page goes back to pending with its attempts reset.
* Cancel: withdraws a page nobody has claimed, or a dead page.
* Clear sent: removes the pages sent.

<h3>Page</h3>

![Message Center page composer: a page to a contact, its destination filled in](../../../doc/img/MessageCenter_plugin_page.png)

The composer of a page, the same as in the Radio Paging modulator: transmitter (its name), protocol, address, type, text and the fields of the protocol. Table opens the codepage table of the text. Post the page queues the page for the transmitter.

* To: the transmitter and the address of the composer, or a contact (one with a send to): its send to for the page (its type, protocol...) fills in the transmitter, the address, the function and the polarity. They can be changed: the page then goes where they say, and To shows the contact whose destination they are, if one has it.
* Edit: the contact chosen (a new one if none). + : the transmitter and the address of the composer to the contacts, as a new contact or added to one: a rule for the messages of the address (and its polarity, if one is set: the same address in the other polarity is another pager) and a send to; there only when the address is one that no contact has as its destination. The list button opens the Contacts tab.

<h3>Contacts</h3>

![Message Center contacts](../../../doc/img/MessageCenter_plugin_contacts.png)

The address book: a name, an icon and a colour for the messages of a pager or a station. The log shows them in the Contact column, and the filters (of the log and of the rules) choose the messages of a contact (Any contact).

![Message Center contacts in the log](../../../doc/img/MessageCenter_plugin_contacts_log.png)

A contact has rules; a message is the contact's when it meets all the conditions of one of them. A condition is on a field of the message: protocol, address, msgType, channel, frequency, mode, or a field of its protocol (ric, function, capcode, group_capcodes...; the details of a message in the log show them all). Its operator is that of the conditions of the filters: is the value (numbers are compared as numbers: the capcode 0001234567 is 1234567), is not, matches a regular expression of the whole value, a comparison of numbers, is present or is absent; a list field (the members of a FLEX group message) when one of its values does. A pager has several addresses (POCSAG functions, FLEX capcodes): one contact with a rule for each. The first contact a message is of names it.

![Message Center contact](../../../doc/img/MessageCenter_plugin_contact.png)

* Icon: a built-in one (Microsoft Fluent Emoji, flat style, MIT License: emergency, radio, messages, weather, places, transport, people, things, signs and colours) or an image file of your own. The colour colours the name; right click on it for none.
* Rules: a row for each condition, its rule number groups them. + Rule adds a rule, + Condition a condition to the rule selected. The field list has the fields of the messages of the log; another one can be typed. Finds: the messages of the log the rule finds; a regular expression that is not valid is shown in red.
* The contacts are saved to and added from a JSON file (a contact of the same name is replaced).

A message of a contact has its name, icon and colour in its data (contact, contact_icon, contact_color): the sinks have them too. A change of the contacts names the messages that come after it.

A page can go to a contact instead of a transmitter and an address (`contact` in a post of the WebAPI or of the MCP tool message_center_post_page). The contact's send to list says how: in order, conditions on the page (protocol, type, text, priority... as a post has them; none for always) and the fields it sets (transmitter, protocol, address, function...). The first one whose conditions the page meets is taken, e.g. voice pages by GSC, binary ones by FLEX, the others by POCSAG. A page no send to takes is refused with the reason.

<h3>Rules</h3>

A rule is a filter (when) and one action (then), shown as a sentence with the number of messages of the log its filter finds; a rule switched off is grey. The actions:

* Log: keep the message in Messages and in the history.
* Send: to a destination of Connections (a file, UDP, a command, speech, the Map).
* Page: to the transmitter of the message, a message for a transmitter becomes a page of the queue (the Send tab). To a contact, or to the contact of the message: the message is paged by the send to of the contact (its transmitter, protocol and address), a relay. For example, POCSAG received from a pager paged again as FLEX: a contact with a rule on the address of the POCSAG pager and a send to of a FLEX transmitter and capcode, and a rule "when a message is POCSAG, RX: page the contact". Select the received messages (RX), or a page heard back by a receiver is paged again. The page is made from the type and the text of the message; the transmitter refuses what it cannot send (the reason is in the Send tab). A contact renamed keeps its page rules; a contact removed removes them.

Pages at given times (a test page every hour, a page sent later) are for cron or a script: they post a page through the WebAPI (the post action of the Message Center, to a contact or to a transmitter and address) or the MCP.
* Mark: an icon before the message, the colour of its text and of its row (each optional). A message several rules mark has their icons side by side.
* Decode as: the message decoded again with another codepage, or as another type.
* Transform: when a regular expression is found in the text, the text becomes a template: ${1} or ${name} for the groups of the expression, ${address}, ${channel}, ${time}, ${text}, ${checksum}... for the fields of the message. E.g. Find Unit:(?<unit>\w+) Job #(?<job>\S+), Into Unit ${unit}: job ${job}.
* Replace: as sed does, each match of a regular expression in the text (or the first one only) replaced, \1 or ${1} for a group: Find (\d{3})(\d{4}), Into \1-\2.
* Drop: the rules after this one do not see the message.

For each message the rules are walked in their number order (#), every one whose filter selects it acting. The changes (mark, decode, transform, replace) are for the rules after them: a filter after a transform sees the new text. The deliveries (log, send, page) are made at the end of the walk, with the message as the changes left it, so the log of rule 1 has the marks of the rules after it; a destination takes a message once. Drop ends the walk; what the rules before it did stays. A transform or a replace keeps the text before in original_text (the details of the message show it). A new Message Center has one rule, 1 "All messages to the log", without a filter: a message no rule logs is not logged. In the WebAPI and the MCP the rules are the routes; the routes of before (a list of sinks, stop, decode) are read as a rule for each, a mark sink as a mark rule.

![Message Center routes: the Emergency preset, all the messages to the log and a mark for each kind of message](../../../doc/img/MessageCenter_plugin_routes.png)

![A route: the messages whose text has brand, fire or smoke, to the mark Fire](../../../doc/img/MessageCenter_plugin_route.png)

A filter on several contacts (set through the WebAPI) is one item of the contact list. Editing a rule keeps what its dialog does not show of its filter. The dialog of a rule is in the order of its sentence: its name, when (the filters, with the number of messages of the log they find as they are changed), then (the action and what it takes) and enabled; a rule that cannot work (no destination, a mark of nothing, an expression that is not valid) says why. Show... (in the dialog, and below the table for the rule selected) opens a window with the messages of the log the rule finds: the columns of Messages, newest first, 100 a page with Newer and Older.

The tool tip of the table of the rules recalls how they work.

* Add (plus icon), Edit (pencil icon, or double click) and Remove (bin icon) manage the rules; a rule can be switched off. Up and Down (arrows) change the order.
* Insert: where a new rule goes, before rule 1 by default, or before or after another.
* Decode as: a message that cannot be decoded so (a codepage of another protocol, a transmitted message) is left as it is. Decode as type is for POCSAG and GSC: a FLEX message is sent with its type, it keeps it.
* Presets: replace the rules and the sinks (after a confirmation; the sources and the contacts stay) with those of a preset. Empty: every message to the log, as a new Message Center. Emergency, an example to adapt: the log, and a mark rule (an icon) for each kind of message, by a few words of its text, in English and in Dutch (the P2000 network): ambulance (ambu, reanimatie, cardiac, drowning), fire (brand, fire, smoke), accident (ongeval, accident, crash), hazardous (gaslucht, hazmat), helicopter (lifeliner, helicopter), police (politie, police), call back (graag contact, please call), plant alarm (high level, fault), test (test page, testoproep). A message of several kinds has their icons side by side.
* Export (save icon) saves the rules, the sinks and the sources to a JSON file; Import (open icon) replaces them with those of a file, after a check and a confirmation.

The filter of a rule has the fields of the filter of the log (quality and conditions too), and the source of the message: the channels, the API (ingest) or a source of Connections. The captures of its text expression are ${1}, ${2}... in the texts of the destination the rule sends to: text Fire at (.*) gives the place as ${1}.

<h3>I/O</h3>

![Message Center sinks and sources: the built in ones and the marks of the Emergency preset, with the messages each took](../../../doc/img/MessageCenter_plugin_io.png)

The sources and sinks: where messages come from besides the channels (files read as they grow, UDP), and where the rules send them besides the log and the queue (files, UDP, commands, speech, the Map). Each has a name and a type. The built in ones come first: the sinks log and queue, the sources channels and api. Double click to edit; a sink renamed is renamed in the rules too, a sink removed removes the rules that send to it. The editor of a sink lists the rules that send to it, and can add a rule that sends every message to it. The table of the sinks gives its limit, the records each took, failed (the dropped ones among them), waiting (held by the limit) and the rules that send to it (none: it takes nothing). Plus adds a sink or a source, bin removes the one selected. The errors of the sinks and sources are shown below them from the first one (a limit reached is said once).

A sink can have a limit: at most so many messages (a command: runs) in a second, a minute or an hour. Over it they are held, in order, and go as the limit allows (at most 100 wait; over that the oldest are dropped), or they are dropped. A new command has 5 a second.

Sinks:

* file: records written to a file, in JSON lines (one record a line, lossless), CSV (the columns of the record, header first), or Pager CSV: the log the Pager demodulator writes, its columns (date, time, address, function bits, alphanumeric, numeric, even parity errors, BCH parity errors, baud, protocol, type, frame) and its messages only, for the tools that read that log. As the Pager demodulator counts them, BCH parity errors are the uncorrectable errors of the message, and even parity errors are 0. Read back, both columns are uncorrectable errors.
* udp: a datagram for each message to an address and port. Pager format: the fields of the Pager demodulator, each ended by a NUL (date, time, address, function bits, alphanumeric, numeric, protocol, type); the address has 7 digits for POCSAG and 9 for FLEX, the function bits are POCSAG only, and the other reading of a message (numeric or alphanumeric) is in the other field. JSON format: the record as one line.
* command: a command run for each message, without a shell. The command is split into the program and its arguments first, then ${address}, ${message}, ${protocol}, ${type}, ${channel}, ${mode}, ${id}, ${frequency}, ${time} (UTC), ${0}, ${1}... (the captures of the rule) and ${field} (any field of the message) are replaced in the arguments, so a message cannot add arguments or change the program: each argument gets the text as it is. ${text:base64}, ${text:json} (a JSON string with its quotes) and ${text:url} encode a value for a program that parses it. Pass: Environment gives the fields in variables MC_ID, MC_TIME, MC_PROTOCOL, MC_ADDRESS, MC_TYPE, MC_TEXT, MC_CHANNEL, MC_FREQUENCY, MC_POLARITY, MC_QUALITY, MC_CONTACT and MC_RECORD (the record as JSON); JSON on its input writes the record as one line of JSON on its standard input (a script reads it, nothing to escape).
* speech: the text, with the same replacements, is said (when SDRangel is built with text to speech).
* map: the position (latitude and longitude) in the text of the message is put on the Map, named by the address.

The actions (command, speech, mark, map) are taken on messages only: not on events, nor on the end of a GSC voice message, which completes its page.

Sources:

* file: a JSON lines file; the lines added to it are read as they are written.
* csv: a CSV log of a message center (the columns of the record) or of the Pager demodulator (Date, Time, Address, Function Bits, Alpha, Numeric...); the rows added are read as they are written. New rows only: the rows already in the file are skipped.
* udp: datagrams received on an address (default 127.0.0.1, this computer only) and port: message records as JSON lines, or the Pager demodulator format. What is received is a message received: it never becomes a page to transmit.

<h3>Options</h3>

In the dialog opened by the button at the right of the tabs:

* Log size: messages kept in the log (default 10000).
* History: keep every message stored in the history (messagecenter/history in the application data directory): a file for each day, up to 4 MB each (YYYYMMDD-NNN.jsonl, a line of JSON for each version of a message), for the days kept (default 30) and the size kept (default 500 MB): the oldest files go. The log is the newest of it when the Message Center starts, and the queries go over all of it, reading only the files of their time range. One Message Center keeps the history.
* Claim timeout: a page given to a transmitter that does not claim it in this time goes back to the queue (default 10 s).
* Silent time: a channel that does not announce itself for this time is offline (default 20 s).

<h2>REST API</h2>

Settings (`MessageCenterSettings`, feature type `MessageCenter`): title, rgbColor, logSize, historyEnabled, historyMaxMB, historyDays, claimTimeoutSec, silentSec, routes, contacts, sinks, sources and the reverse API settings. Routes, contacts, sinks and sources are JSON arrays (as text); a setting with a sink, source, route or contact that is not valid is refused with the reason. A contact: `{"name": "Fire station 3", "icon": ":fire_engine", "color": 4294926400, "rules": [[{"field": "protocol", "is": "pocsag"}, {"field": "ric", "is": "1234567"}], [{"field": "protocol", "is": "flex"}, {"field": "capcode", "matches": "11000.."}]]}`.

Report (`MessageCenterReport`): the number of messages logged and routed, the endpoints, the pages, the log, the records each sink took and refused (sinks), the errors, and the result of the last query (query).

Actions (`MessageCenterActions`):

* `post`: a page to transmit (`MessageCenterPost`): transmitter (its name), protocol, address, type, text and the fields of the protocol. The FLEX address takes the capcode forms of STD-43A Appendix A.
* `requeue`, `cancel`: the id of a page.
* `clearSent`: 1 to remove the pages sent.
* `redecode`: a record of the log decoded again (`MessageCenterRedecode`: id, charset, type).
* `ingest`: message records as JSON lines, routed as messages received. A record needs its protocol; one without a timestamp gets the time it is given, one without an id gets an id of its own.
* `importFile`: the path of a JSON lines or CSV file read into the log, not routed.
* `query`: a query of the messages as JSON, over the history (the log without one): `from` and `to` (ms since the epoch) or `last` (seconds back from now), `filter` (as the filter of a route), `offset` and `limit` (default 100, at most 1000). Its result is in the report (query): the query (from and to resolved, to ask for the next page with them), total (the messages it selects) and records (the page, newest first). E.g. `{"last": 86400, "filter": {"protocols": ["flex"], "text": "fire"}, "limit": 50}`.

<h2>MCP</h2>

The MCP server feature has tools for the Message Center: `get_message_center` (routes, sinks, sources, channels, queue, sink counts, errors and the newest messages of the log), `set_message_center_routing` (routes, sinks and sources), `message_center_post_page`, `message_center_page_action` (requeue, cancel, clear sent), `message_center_ingest`, `message_center_import_file` and `message_center_query` (the query action: the total and a page of the messages of a time range and a filter, over the history). `get_paging_messages` gives the last 1000 messages of the paging channels whether or not a Message Center is there.
