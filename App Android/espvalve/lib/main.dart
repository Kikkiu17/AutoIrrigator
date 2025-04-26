import 'package:flutter/material.dart';
import 'dart:io' show  Platform;
import 'package:window_size/window_size.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'homepage.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  //await Permission.storage.request().isGranted;

  final SharedPreferences storage = await SharedPreferences.getInstance();

  if (Platform.isWindows || Platform.isLinux || Platform.isMacOS) {
    setWindowTitle('ESPIOT');
    setWindowMaxSize(const Size(650, 830));
    setWindowMinSize(const Size(650, 830));
  }
  runApp(MainApp(storage: storage));
}

class MainApp extends StatelessWidget {
  final SharedPreferences storage;

  const MainApp({super.key, required this.storage});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'ESPVALVE',
      theme: ThemeData(
      useMaterial3: true,
      colorScheme: ColorScheme.fromSeed(seedColor: const Color.fromARGB(255, 34, 192, 255)),
      ),
      home: HomePage(storage: storage),
    );
  }
}
