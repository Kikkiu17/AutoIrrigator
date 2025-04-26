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
      _socket = await Socket.connect(ip, port).timeout(Duration(milliseconds: timeout));
      socketListen();
    } catch (e) {
      return false;
    }
    return true;
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

  Card _addSwitchFeature(String feature)
  {
    // switch1:Valvola1,sensor:Stato:aperta,sensor:Litri/s:5.24;
    feature.replaceAll(";", "");
    String switchId = feature.split(":")[0][feature.split(":")[0].length - 1];
    String switchname = feature.split(":")[1].split(",")[0];
    String text = "$switchId: $switchname";

    for (String addon in feature.split(","))
    {
      if (addon.contains("sensor"))
      {
        String sensorName = addon.split(":")[1];
        String sensorData = addon.split(":")[2];
        text += " - $sensorName: $sensorData";
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
                color: Theme.of(context).colorScheme.inversePrimary,
                child: InkWell(
                  enableFeedback: true,
                  child: SizedBox(
                    height: 50,
                    width: 50,
                    child: Icon(Icons.radio_button_checked, color: Theme.of(context).colorScheme.primary)  // Theme.of(context).colorScheme.primary
                    ),
                    onTap: () async {
                      update = false;

                      while (await Future.delayed(const Duration(milliseconds: 10), () => widget.device.updatingValues)) {
                        // wait for the update to finish
                      }

                      widget.device.openCloseValve(switchId).then((statusCode) {
                        if (statusCode != "200 OK") {
                          showPopupOK(context, "Errore", "Impossibile inviare il comando. Riprova.");
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
    //String timestampId = feature.split(":")[0][feature.split(":")[0].length - 1];
    String timestampName = feature.split(":")[1];
    String timestamp = feature.split(":")[2];
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
    //String sensorId = feature.split(":")[0][feature.split(":")[0].length - 1];
    String sensorName = feature.split(":")[1];
    String sensorData = feature.split(":")[2];
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
