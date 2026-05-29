import 'package:flutter/material.dart';

import 'i18n/strings.dart';
import 'ui/home_screen.dart';

class AaBridgeApp extends StatelessWidget {
  const AaBridgeApp({super.key});

  @override
  Widget build(BuildContext context) {
    final locale = LocaleScope.of(context).locale;
    return MaterialApp(
      title: 'AA Bridge',
      debugShowCheckedModeBanner: false,
      locale: locale,
      supportedLocales: supportedLocales,
      theme: ThemeData(
        useMaterial3: true,
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFF1B6CFF),
          brightness: Brightness.dark,
        ),
      ),
      home: const HomeScreen(),
    );
  }
}
