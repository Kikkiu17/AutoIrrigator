import 'dart:io' show Socket;

import 'package:espvalve/discovery.dart';
import 'package:flutter/material.dart';
import 'device.dart';
import 'package:flashy_tab_bar2/flashy_tab_bar2.dart';
import 'package:shared_preferences/shared_preferences.dart';

class HomePage extends StatefulWidget
{
  final SharedPreferences storage;
  const HomePage({super.key, required this.storage});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  Socket? socket;
  static List<Widget> _cardList = List.empty(growable: true);
  static Widget pageToRender = const Text('Caricamento...');

  static bool _updateExistingIDs = true;

  static List<String> _existingIPandIDs = List.empty(growable: true);

  static DevicePage? page;
  static Device _selectedDevice = Device();
  static int _selectedIndex = 0;

@override
  void initState() {
    _createDeviceList();
    super.initState();
  }

@override
  void dispose() {
    super.dispose();
  }

  void _saveItem(List<String> ids) async {
    await widget.storage.setStringList("devices", ids);
  }

  Future<List<String>> _getData() async {
    var item = widget.storage.getStringList("devices");
    if (item == null) return List.empty();
    return item.cast<String>();
  }

  void _createDeviceList() async
  {
   if (_updateExistingIDs) {
      _updateExistingIDs = false;
      _existingIPandIDs = await _getData();
    }

    // svuota la lista dei dispositivi durante il caricamento
    _cardList = [
      const ListTile(
        title: Padding(
          padding: EdgeInsets.all(8.0),
          child: Text("Caricamento..."),
        )
      )
    ];

    // cerca nuovi dispositivi e crea le tile
    discoverDevices(_existingIPandIDs).then((deviceList) {
      _existingIPandIDs = List.empty(growable: true);
      List<Widget> cardList = List.empty(growable: true);
      List<String> ipsAndIds = List.empty(growable: true);
      for (Device dev in deviceList) {
        cardList.add(_createDeviceTile(dev, context));
        ipsAndIds.add("${dev.ip};${dev.id}");
      }

      cardList.add(
        const Padding(padding: EdgeInsets.only(top: 8.0))
      );

      // crea il pulsante di aggiornamento della lista
      cardList.add(
      ListTile(
          title: ElevatedButton(
            child: const Text("Aggiorna"),
            onPressed: () {
              setState(() {
                _updateExistingIDs = true;
                _createDeviceList();
              });
            }
          )
        )
      );

      // crea il pulsante di discovery
      cardList.add(
      ListTile(
          title: ElevatedButton(
            child: const Text("Trova nuovi dispositivi"),
            onPressed: () {
              setState(() {
                _updateExistingIDs = false;
                _createDeviceList();
              });
            }
          )
        )
      );

      _saveItem(ipsAndIds);  // salva la lista di dispositivi trovati

      // effettua il rebuild del widget con la nuova lista
      setState(() {
        _cardList = cardList;
      });
    });
  }

  ListTile _createDeviceTile(Device device, BuildContext context) {
    return ListTile(
      tileColor: (device.name == "OFFLINE") ? const Color.fromARGB(255, 228, 228, 228) : Theme.of(context).colorScheme.surface,
      shape: RoundedRectangleBorder(
        side: const BorderSide(width: 0.8),
        borderRadius: BorderRadius.circular(20),
      ),
      leading: CircleAvatar(
        backgroundColor: (device.name == "OFFLINE") ? const Color.fromARGB(255, 182, 182, 182) : null,
        child: const Icon(Icons.sensors)
      ),
      title: Text(device.name),
      subtitle: Text("${device.id} | ${device.ip}"),
      trailing: InkWell(
        child: SizedBox(
          height: 50,
          width: 50,
          child: Icon(Icons.edit, color: (device.name == "OFFLINE") ? Colors.grey : Theme.of(context).colorScheme.primary)  // Theme.of(context).colorScheme.primary
        ),
        onTap: () {
          // bottone di modifica del nome del dispositivo
          // se il dispositivo è offline, mostra un popup di errore
          if (device.name == "OFFLINE") {
            showPopupOK(context, "Impossibile effettuare l'azione", "Il dispositivo sembra essere offline. prova ad aggiornare la lista.");
          } else {
            device.changeName(context).then((value) {
              setState(() {
                //_updateExistingIDs = true;
                _createDeviceList();
              });
            });
          }
        },
      ),
      enableFeedback: true,
      onTap: () {
        // bottone di selezione del dispositivo
        // se il dispositivo è offline, mostra un popup di errore
        if (device.name == "OFFLINE") {
          showPopupOK(context, "Impossibile effettuare l'azione", "Il dispositivo sembra essere offline. prova ad aggiornare la lista.");
        } else {
          page = device.setThisDevicePage();
          if (page.runtimeType == DevicePage) {
            setState(() {
              _selectedIndex = 1;
              _selectedDevice = device;
            });
          }
        }
      },
    );
  }

  @override
  Widget build(BuildContext context) {
    if (_selectedIndex == 1 && page.runtimeType == DevicePage) {
      pageToRender = page!;
    }
    else {
      pageToRender = ListView(
        children: _cardList,
      );
    }

    return Scaffold (
      appBar: AppBar(
        title: _selectedIndex == 0 ? const Text("Dispositivi") : Text(_selectedDevice.name),
      ),
      body: Center(child: FractionallySizedBox(widthFactor: 0.95, child: pageToRender)),
      bottomNavigationBar: FlashyTabBar(
        animationCurve: Curves.linear,
        selectedIndex: _selectedIndex,
        iconSize: 30,
        showElevation: false,
        onItemSelected: (index) => setState(() {
          _selectedIndex = index;
        }),
        items: [
          FlashyTabBarItem(
            icon: const Icon(Icons.list),
            title: const Text('Lista'),
          ),
          FlashyTabBarItem(
            icon: const Icon(Icons.sensors),
            title: const Text('Dispositivo'),
          ),
        ],
      ),
    );
  }
}
