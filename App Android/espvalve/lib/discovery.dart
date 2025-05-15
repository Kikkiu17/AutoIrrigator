import 'dart:async';
import 'device.dart';
import 'dart:io';
import 'package:network_info_plus/network_info_plus.dart';

const int maxIp = 64; // max number of IPs to scan
const int scanTimeout = 30; // ms
const String idTemplate = "ESPDEVICE";
int timeout = 1000; // ms
const int defaultPort = 34677; // default port for ESP devices

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

Future<List<String>> scanNetwork() async {
    List<String> ips = List.empty(growable: true);
    await (NetworkInfo().getWifiIP()).then(
      (ip) async {
        final String subnet = ip!.substring(0, ip.lastIndexOf('.'));
        for (var i = 0; i < maxIp; i++) {
          String ip = '$subnet.$i';
          await Socket.connect(ip, defaultPort, timeout: Duration(milliseconds: scanTimeout))
            .then((socket) async {
              ips.add(socket.address.address);
              socket.destroy();
            }).catchError((error) => null);
        }
      },
    );
    return ips;
  }

Future<List<Device>> discoverDevices(List<String> ips) async
{
  List<Device> devices = List.empty(growable: true);
  bool newList = false;

  if (ips.isEmpty) {
    // if no ids are provided, discover all devices in the range
    newList = true;
    ips = await scanNetwork();
  }

  for (String ip in ips)
  {
    // ip;id
    ip = ip.split(";")[0];
    ESPSocket espSocket = ESPSocket();

    bool connected = await espSocket.connect(ip, defaultPort);

    if (!connected) {
      if (!newList) {
        // il dispositivo non è raggiungibile, lo aggiungo come offline
        // se non è una lista nuova (!newList), 
        Device device = Device();
        device.id = "";
        device.ip = ip;
        device.name = "OFFLINE";
        devices.add(device);
      }
      continue;
    }

    String response = await espSocket.sendAndWaitForAnswerTimeout("GET ?wifi=IP");
    if (!response.contains("200 OK")) {
      // retry
      response = await espSocket.sendAndWaitForAnswerTimeout("GET ?wifi=IP");
    }
    
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
