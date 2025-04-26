import 'dart:async';
import 'device.dart';

const int lowerId = 1;
const int upperId = 5;
const String idTemplate = "ESPDEVICE";
int timeout = 250; // ms
const int defaultPort = 23; // default port for ESP devices

Future<Device> createDevice(String ip, ESPSocket socket) async
{
  Device device = Device();
  device.ip = ip;

  // --- GET DEVICE ID ---
  await Future.delayed(const Duration(milliseconds: 10));
  String response = await socket.sendAndWaitForAnswerTimeout("GET ?wifi=ID");
  if (!response.contains("200 OK")) {
    return device;
  }
  device.id = response.split("\n")[1];

  // --- GET DEVICE NAME ---
  await Future.delayed(const Duration(milliseconds: 10));
  response = await socket.sendAndWaitForAnswerTimeout("GET ?wifi=name");
  if (!response.contains("200 OK")) {
    return device;
  }
  device.name = response.split("\n")[1];
  
  // --- GET DEVICE FEATURES ---
  await Future.delayed(const Duration(milliseconds: 10));
  response = await socket.sendAndWaitForAnswerTimeout("GET ?features");
  if (!response.contains("200 OK")) {
    return device;
  }
  device.features = response.split("\n")[1].split(";");

  return device;
}

Future<List<Device>> discoverDevices(List<String> idsAndIps) async
{
  List<Device> devices = List.empty(growable: true);
  bool newList = false;

  if (idsAndIps.isEmpty) {
    // if no ids are provided, discover all devices in the range
    newList = true;
    idsAndIps = List.generate(upperId - lowerId + 1, (index) => "ESPDEVICE${(lowerId + index).toString().padLeft(3, '0')}");
  }

  for (String idAndIp in idsAndIps)
  {
    // idsAndIp: IP;ID
    String ip = "";
    String id = "";
    String host = "";
    if (newList) {
      id = idAndIp;
      host = id;
    } else {
      ip = idAndIp.split(";")[0];
      id = idAndIp.split(";")[1];
      host = ip;
    }
    ESPSocket espSocket = ESPSocket();

    if (!await espSocket.connect(host, defaultPort)) {
      if (!newList) {
        // il dispositivo non è raggiungibile, lo aggiungo come offline
        // se non è una lista nuova (!newList), 
        Device device = Device();
        device.id = id;
        device.ip = ip;
        device.name = "OFFLINE";
        devices.add(device);
      }
      continue;
    }

    String response = await espSocket.sendAndWaitForAnswerTimeout("GET ?wifi=IP");
    if (response.contains("200 OK")) {
      ip = response.split("\n")[1];
    } else {
      espSocket.close();
      continue;
    }

    Device device = await createDevice(ip, espSocket);
    espSocket.close();

    devices.add(device);
  }

  return devices;
}
