import 'dart:io';

import 'package:flutter/material.dart';
import 'discovery.dart';

import 'dart:convert';
import 'dart:async';

const int updateTime = 250; // ms
bool update = true;

void showPopupOK(BuildContext context, String title, String content) {
  showDialog(
    context: context,
    builder: (BuildContext context) => AlertDialog(
      title: Text(title),
      content: Text(content),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context, 'OK'),
          child: const Text('OK'),
        ),
      ],
    )
  );
}

class ESPSocket {
  late Socket _socket;
  Completer _completer = Completer();

  void socketListen() {
    _socket.listen(
      (event) {
        if (_completer.isCompleted) {
          _completer = Completer();
        }
        _completer.complete(event);
      },
    );
  }

  Future<String> sendAndWaitForAnswer(data) async {
    _socket.write(data);
    var answer = await _completer.future;
    _completer = Completer();
    answer = utf8.decode(answer);
    return answer;
  }

  Future<String> sendAndWaitForAnswerTimeout(data) async {
    _socket.write(data);
    try {
      var answer = await _completer.future.timeout(Duration(milliseconds: timeout));
      _completer = Completer();
      answer = utf8.decode(answer);
      return answer;
    } catch (e) {
      return "";
    }
  }

  Future<String> getAnswerTimeout() async {
    await _socket.flush();
    try {
      var answer = await _completer.future.timeout(Duration(milliseconds: timeout));
      _completer = Completer();
      answer = utf8.decode(answer);
      return answer;
    } catch (e) {
      return "";
    }
  }

  Future<bool> connect(String ip, int port) async {
    try {
      _socket = await Socket.connect(ip, port, timeout: Duration(milliseconds: timeout));
      socketListen();
      return true;
    } catch (e) {
      return false;
    }
    /*.then((socket) {
      _socket = socket;
      socketListen();
      return true;
    }).catchError((error) => false);
    return false;*/
  }

  Future<void> flush() async {
    await _socket.flush();
    _completer = Completer();
  }

  Future<void> close() async {
    await _socket.flush();
    await _socket.close();
  }
}

class Device {
  String id = "";
  String name = "";
  String ip = "";
  List<String> features = List.empty(growable: true);
  final ESPSocket _espsocket = ESPSocket();
  bool updatingValues = false;

  Future<String> sendName(String name) async {
    while (!await _espsocket.connect(ip, defaultPort)) {}
    await _espsocket.flush();
    String resp = await _espsocket.sendAndWaitForAnswerTimeout("POST ?wifi=changename&name=$name");
    await _espsocket.close();
    return resp.split("\n")[0];
  }

  Future<bool?> changeName(BuildContext context) async {
    String userInputName = "";

    bool? ret = await showDialog (
      context: context,
      builder: (BuildContext context) => AlertDialog(
        title: const Text("Cambia nome"),
        content: TextField(
          decoration: const InputDecoration(
            border: OutlineInputBorder(),
            hintText: "Modifica il nome del dispositivo",
          ),
          onChanged: (text) {
            userInputName = text;
          },
        ),
        actions: [
          TextButton(
            onPressed: () {

              sendName(userInputName).then((statusCode) {
                if (statusCode == "200 OK") {
                  showPopupOK(context, "Successo", "Nome modificato con successo.");
                }
                else {
                  showPopupOK(context, "Errore", "Impossibile modificare il nome del dispositivo. Riprova.");
                }
              });

              Navigator.pop(context, true);
            },
            child: const Text('OK'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Annulla'),
          )
        ],
      )
    );

    return ret;
  }

  Future<void> getFeatures() async {
    String response = await _espsocket.sendAndWaitForAnswerTimeout("GET ?features");
    if (!response.contains("200 OK") || response == "") {
      return;
    }
    features = response.split("\n")[1].split(";");
  }

  DevicePage setThisDevicePage() {
    return DevicePage(device: this);
  }

  // FUNZIONI PERSONALIZZATE PER L'APPLICAZIONE SPECIFICA
  // FUNZIONI PULSANTE
  Future<String> openCloseValve(String id) async {
    await _espsocket.flush();
    String resp = await _espsocket.sendAndWaitForAnswerTimeout("GET ?valve=$id");
    if (resp.contains("200 OK")) {
      String isOpen = resp.split("\n")[1];
      if (isOpen == "aperta") {
      _espsocket.sendAndWaitForAnswerTimeout("POST ?valve=$id&cmd=close");
      } else if (isOpen == "chiusa") {
      _espsocket.sendAndWaitForAnswerTimeout("POST ?valve=$id&cmd=open");
      }
    }

    return resp.split("\n")[0];
  }
}

class DevicePage extends StatefulWidget {
  const DevicePage({
    super.key,
    required this.device
  });

  final Device device;

  @override
  State<DevicePage> createState() => _DevicePageState();
}

final ValueNotifier<bool> generateIOs = ValueNotifier(false);
final ValueNotifier<bool> updateIOs = ValueNotifier(false);

class _DevicePageState extends State<DevicePage> {
  late Timer _timer;
  List<Widget> _userIOs = List.empty(growable: true);

  void _generateIOsListener() {
    if (generateIOs.value) {
      updateIOs.value = false;
      _userIOs = _generateDirectUserIOs(widget.device);
      updateIOs.value = true;
      generateIOs.value = false;
    }
  }

  @override
  void initState() {
    super.initState();
    
    generateIOs.addListener(_generateIOsListener);

    widget.device._espsocket.connect(widget.device.ip, defaultPort).then((connected) => {
      if (connected) {
        _timer = Timer.periodic(const Duration(milliseconds: updateTime), (timer) {
          updateDirectUserIOs(widget.device);
        }),

        _userIOs = _generateDirectUserIOs(widget.device),
      }
    });

  }

  @override
  void dispose() {
    _timer.cancel();
    generateIOs.removeListener(_generateIOsListener);
    widget.device._espsocket.close();
    super.dispose();
  }

  // DATA SEPARATOR
  // for example: switch1<DATA SEPARATOR>Valvola1,sensor<DATA SEPARATOR>Stato<DATA SEPARATOR>aperta,sensor<DATA SEPARATOR>Litri/s<DATA SEPARATOR>5.24;

  final String _dataSeparator = "\$";

  Card _addSwitchFeature(String feature)
  {
    // switch1$Valvola1,status$1,sensor$Stato$aperta,sensor$Litri/s$5.24;
    feature.replaceAll(";", "");
    String switchId = feature.split(_dataSeparator)[0][feature.split(_dataSeparator)[0].length - 1];
    String switchname = feature.split(_dataSeparator)[1].split(",")[0];
    String text = "$switchId: $switchname";
    Color color = Theme.of(context).colorScheme.inversePrimary;

    for (String addon in feature.split(","))
    {
      if (addon.contains("sensor")) {
        String sensorName = addon.split(_dataSeparator)[1];
        String sensorData = addon.split(_dataSeparator)[2];
        text += " - $sensorName: $sensorData";
      } else if (addon.contains("status")) {
        String status = addon.split(_dataSeparator)[1];
        if (status == "0") {
          color = const Color.fromARGB(255, 255, 187, 187);
        } else if (status == "1") {
          color = const Color.fromARGB(255, 187, 255, 187);
        }
      }
    }

    return Card(
      color: Theme.of(context).cardColor,
      child: Padding(
        padding: const EdgeInsets.all(5),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Padding(
              padding: const EdgeInsets.only(left: 8.0),
              child: Text(text),
            ),
            Padding(
              padding: const EdgeInsets.only(right: 3),
              child: Card(
                color: color,
                child: InkWell(
                  enableFeedback: true,
                  child: SizedBox(
                    height: 50,
                    width: 50,
                    child: Icon(Icons.radio_button_checked, color: Color.alphaBlend(Colors.black.withAlpha(220), color))  // Theme.of(context).colorScheme.primary
                    ),
                    onTap: () async {
                      update = false;

                      while (await Future.delayed(const Duration(milliseconds: 10), () => widget.device.updatingValues)) {
                        // wait for the update to finish
                      }

                      widget.device.openCloseValve(switchId).then((statusCode) {
                        if (statusCode != "200 OK") {
                          showPopupOK(context, "Errore", "Impossibile inviare il comando:\n$statusCode");
                        }
                        update = true;
                      });
                    },
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Card _addTimestampFeature(String feature)
  {
    // timestamp1:TimestampName:timestamp;
    feature.replaceAll(";", "");
    //String timestampId = feature.split(_dataSeparator)[0][feature.split(_dataSeparator)[0].length - 1];
    String timestampName = feature.split(_dataSeparator)[1];
    String timestamp = feature.split(_dataSeparator)[2];
    String text = "$timestampName: $timestamp";

    return Card(
      color: Theme.of(context).cardColor,
      child: Padding(
        padding: const EdgeInsets.all(5),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Padding(
              padding: const EdgeInsets.only(left: 8.0),
              child: Text(text),
            ),
          ],
        ),
      ),
    );
  }

  Card _addSensorFeature(String feature)
  {
    // sensor1:SensorName:SensorData;
    feature.replaceAll(";", "");
    //String sensorId = feature.split(_dataSeparator)[0][feature.split(_dataSeparator)[0].length - 1];
    String sensorName = feature.split(_dataSeparator)[1];
    String sensorData = feature.split(_dataSeparator)[2];
    String text = "";
    //text += "$sensorId: ";
    text += "$sensorName: $sensorData";

    return Card(
      color: Theme.of(context).cardColor,
      child: Padding(
        padding: const EdgeInsets.all(5),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Padding(
              padding: const EdgeInsets.only(left: 8.0),
              child: Text(text),
            ),
          ],
        ),
      ),
    );
  }

  Card _addButtonFeature(String feature)
  {
    // button1&giao&dataToSend;
    feature.replaceAll(";", "");
    //String buttonId = feature.split(_dataSeparator)[0][feature.split(_dataSeparator)[0].length - 1];
    String buttonText = feature.split(_dataSeparator)[1];
    String dataToSend = "";
    if (feature.split(_dataSeparator).length == 3) {
      dataToSend = feature.split(_dataSeparator)[2];
    }

    return Card(
      color: Theme.of(context).cardColor,
      child: Padding(
        padding: const EdgeInsets.all(5),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Padding(
              padding: const EdgeInsets.only(left: 8.0),
              child: ElevatedButton(
                style: ElevatedButton.styleFrom(
                    backgroundColor: Color.alphaBlend(Colors.white.withAlpha(150), Theme.of(context).colorScheme.inversePrimary),
                  foregroundColor: Theme.of(context).colorScheme.primary,
                ),
                child: Text(buttonText),
                onPressed: () {
                  if (dataToSend == "") {
                    showPopupOK(context, "Errore", "Nessun contenuto inviato.");
                  } else {
                    widget.device._espsocket.sendAndWaitForAnswer(dataToSend).then((statusCode) {
                      if (statusCode != "200 OK") {
                        showPopupOK(context, "Errore", "Impossibile inviare il comando:\n$statusCode");
                      }
                    });
                  }
                }
              ),
            ),
          ],
        ),
      ),
    );
  }

  List<TextEditingController> textInputControllers = List.empty(growable: true);

  Card _addTextInputFeature(String feature)
  {
    // textinput1$08:00-09:30,button$Imposta$send;
    feature.replaceAll(";", "");
    List<Widget> addons = List.empty(growable: true);
    int textInputId = int.parse(feature.split(_dataSeparator)[0][feature.split(_dataSeparator)[0].length - 1]);
    String textInputHintText = "";
    if (feature.contains(",")) {
      textInputHintText = feature.split(",")[0].split(_dataSeparator)[1];
    } else {
      textInputHintText = feature.split(_dataSeparator)[1];
    }

    String dataToSend = feature.split(_dataSeparator)[3];
    TextEditingController textInputController = TextEditingController();
    if (textInputControllers.length >= textInputId) {
      textInputController = textInputControllers[textInputId - 1];
    } else {
      textInputControllers.add(textInputController);
    }

    for (String addon in feature.split(","))
    {
      if (addon.contains("button"))
      {
        String buttonText = addon.split(_dataSeparator)[1];
        addons.add(
          ElevatedButton(
            style: ElevatedButton.styleFrom(
                backgroundColor: Color.alphaBlend(Colors.white.withAlpha(150), Theme.of(context).colorScheme.inversePrimary),
              foregroundColor: Theme.of(context).colorScheme.primary,
            ),
            child: Text(buttonText),
            onPressed: () async {
              update = false;
              while (await Future.delayed(const Duration(milliseconds: 10), () => widget.device.updatingValues)) {
                // wait for the update to finish
              }

              if (dataToSend.startsWith("send")) {
                // dataToSend: sendPOST ?key=<TEXTINPUT>
                String template = dataToSend.split("send")[1];
                widget.device._espsocket.sendAndWaitForAnswer("$template${textInputController.text}").then((statusCode) {
                  if (statusCode.split("\n")[0] != "200 OK") {
                    showPopupOK(context, "Errore", "Impossibile inviare il comando:\n$statusCode");
                  }
                  textInputController.clear();
                  update = true;
                });
              } else {
                if (dataToSend == "") {
                  WidgetsBinding.instance.addPostFrameCallback((_) {
                    showPopupOK(context, "Errore", "Nessun contenuto inviato.");
                  });
                } else {
                  widget.device._espsocket.sendAndWaitForAnswer(dataToSend).then((statusCode) {
                    if (statusCode.split("\n")[0] != "200 OK") {
                      showPopupOK(context, "Errore", "Impossibile inviare il comando:\n$statusCode");
                    }
                    update = true;
                  });
                }
              }
            }
          ),
        );
      }
    }

    return Card(
      color: Theme.of(context).cardColor,
      child: Padding(
        padding: const EdgeInsets.all(5),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Expanded( // Ensures the TextField takes up available space
              child: TextField(
                controller: textInputController,
                decoration: InputDecoration(
                  //border: const OutlineInputBorder(),
                  hintText: textInputHintText,
                ),
              ),
            ),
            ...addons
          ],
        ),
      ),
    );
  }

  List<List<String>> timePickerData = List.empty(growable: true);

  String padLeft(String input, int totalWidth, String paddingChar) {
    if (input.length >= totalWidth) {
      return input;
    }
    return paddingChar * (totalWidth - input.length) + input;
  }

  Card _addTimePickerFeature(String feature)
  {
    // "timepicker1$22:00-23:00,button$Imposta$sendPOST ?valve=1&schedule=;"
    feature.replaceAll(";", "");
    List<Widget> addons = List.empty(growable: true);
    int timePickerId = int.parse(feature.split(_dataSeparator)[0][feature.split(_dataSeparator)[0].length - 1]);
    String dataToSend = feature.split(_dataSeparator)[3];
    List<String> receivedTime = List<String>.filled(2, "00:00");
    TimeOfDay startTime;
    TimeOfDay endTime;
    String timePickerReceivedData;
    if (feature.contains(",")) {
      timePickerReceivedData = feature.split(",")[0].split(_dataSeparator)[1];
      if (timePickerReceivedData != "") {
        receivedTime = timePickerReceivedData.split("-");
      }
    } else {
      timePickerReceivedData = feature.split(_dataSeparator)[1];
      if (timePickerReceivedData != "") {
        receivedTime = timePickerReceivedData.split("-");
      }
    }

    if (timePickerData.length >= timePickerId) {
      receivedTime = timePickerData[timePickerId - 1];
    } else {
      timePickerData.add(receivedTime);
    }

    if (receivedTime.length < 2) {
      startTime = TimeOfDay.now();
      endTime = TimeOfDay.now();
    } else {
      startTime = TimeOfDay(
      hour: int.parse(receivedTime[0].split(":")[0]),
      minute: int.parse(receivedTime[0].split(":")[1]),
      );
      endTime = TimeOfDay(
      hour: int.parse(receivedTime[1].split(":")[0]),
      minute: int.parse(receivedTime[1].split(":")[1]),
      );
    }

    for (String addon in feature.split(","))
    {
      if (addon.contains("button"))
      {
        String buttonText = addon.split(_dataSeparator)[1];
        addons.add(
          ElevatedButton(
            style: ElevatedButton.styleFrom(
                backgroundColor: Color.alphaBlend(Colors.white.withAlpha(150), Theme.of(context).colorScheme.inversePrimary),
              foregroundColor: Theme.of(context).colorScheme.primary,
            ),
            child: Text(buttonText),
            onPressed: () async {
              update = false;
              while (await Future.delayed(const Duration(milliseconds: 10), () => widget.device.updatingValues)) {
                // wait for the update to finish
              }

              if (dataToSend.startsWith("send")) {
                // dataToSend: sendPOST ?key=<TEXTINPUT>
                String template = dataToSend.split("send")[1];
                String timeToSend = "${padLeft("${startTime.hour}", 2, "0")}:${padLeft("${startTime.minute}", 2, "0")}-${padLeft("${endTime.hour}", 2, "0")}:${padLeft("${endTime.minute}", 2, "0")}";
                widget.device._espsocket.sendAndWaitForAnswer("$template$timeToSend").then((statusCode) {
                  if (statusCode.split("\n")[0] != "200 OK") {
                    showPopupOK(context, "Errore", "Impossibile inviare il comando:\n$statusCode");
                  }
                  update = true;
                });
              } else {
                if (dataToSend == "") {
                  WidgetsBinding.instance.addPostFrameCallback((_) {
                    showPopupOK(context, "Errore", "Nessun contenuto inviato.");
                  });
                } else {
                  widget.device._espsocket.sendAndWaitForAnswer(dataToSend).then((statusCode) {
                    if (statusCode.split("\n")[0] != "200 OK") {
                      showPopupOK(context, "Errore", "Impossibile inviare il comando:\n$statusCode");
                    }
                    update = true;
                  });
                }
              }
            }
          ),
        );
      }
    }

    Color timePickerColor;
    if (timePickerReceivedData != "${receivedTime[0]}-${receivedTime[1]}") {
        timePickerColor = const Color.fromARGB(255, 255, 187, 187);
      } else {
        timePickerColor = const Color.fromARGB(255, 243, 243, 243);
      }

    return Card(
      color: Theme.of(context).cardColor,
      child: Padding(
        padding: const EdgeInsets.all(5),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.start,
              children: [
                ElevatedButton(
                  style: ElevatedButton.styleFrom(
                  backgroundColor: timePickerColor,
                  foregroundColor: Colors.black,
                  shape: const RoundedRectangleBorder(
                    borderRadius: BorderRadius.all(Radius.circular(5)),
                  ),
                  ),
                  child: Text("${padLeft("${startTime.hour}", 2, "0")}:${padLeft("${startTime.minute}", 2, "0")}"),
                  onPressed: () async {
                  TimeOfDay? newTime = await showTimePicker(
                    initialEntryMode: TimePickerEntryMode.dial,
                    context: context,
                    initialTime: startTime,
                  );
                  if (newTime != null) {
                    timePickerData[timePickerId - 1][0] = "${padLeft("${newTime.hour}", 2, "0")}:${padLeft("${newTime.minute}", 2, "0")}";
                  }
                  },
                ),
                const SizedBox(width: 8), // Add spacing between buttons
                ElevatedButton(
                  style: ElevatedButton.styleFrom(
                  backgroundColor: timePickerColor,
                  foregroundColor: Colors.black,
                  shape: const RoundedRectangleBorder(
                    borderRadius: BorderRadius.all(Radius.circular(5)),
                  ),
                  ),
                  child: Text("${padLeft("${endTime.hour}", 2, "0")}:${padLeft("${endTime.minute}", 2, "0")}"),
                  onPressed: () async {
                  TimeOfDay? newTime = await showTimePicker(
                    initialEntryMode: TimePickerEntryMode.dial,
                    context: context,
                    initialTime: endTime,
                  );
                  if (newTime != null) {
                    timePickerData[timePickerId - 1][1] = "${padLeft("${newTime.hour}", 2, "0")}:${padLeft("${newTime.minute}", 2, "0")}";
                  }
                  },
                ),
              ],
            ),
            ...addons
          ],
        ),
      ),
    );
  }

  List<Widget> _generateDirectUserIOs(Device device)
  {
    List<Widget> widgetList = List.empty(growable: true);
    
    for (String feature in device.features)
    {
      /**
       * example of feature:
       * switch1:Valvola1,sensor:Stato:aperta,sensor:Litri/s:5.24;
       * timestamp1:Tempo CPU:123 ms;
       */

      if (feature.startsWith("switch")) {
        widgetList.add(_addSwitchFeature(feature));
      } else if (feature.startsWith("timestamp")) {
        widgetList.add(_addTimestampFeature(feature));
      } else if (feature.startsWith("sensor")) {
        widgetList.add(_addSensorFeature(feature));
      } else if (feature.startsWith("button")) {
        widgetList.add(_addButtonFeature(feature));
      } else if (feature.startsWith("textinput")) {
        widgetList.add(_addTextInputFeature(feature));
      } else if (feature.startsWith("timepicker")) {
        widgetList.add(_addTimePickerFeature(feature));
      }
    }

    return widgetList;
  }
  
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: ListView(
        children: [
          ValueListenableBuilder(
            valueListenable: updateIOs,
            builder: (context, updateIOs, child) {
              return Column(
                 children: [..._userIOs]
              );
            }
          ),
        ]
      )
    );
  }
}

void updateDirectUserIOs(Device dev) async {
  if (update) {
    dev.updatingValues = true;
    await dev.getFeatures();
    generateIOs.value = true;
    dev.updatingValues = false;
  }
}
