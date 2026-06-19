// ============================================================
// wirhost.exe - Запуск WirPE Installer
// namespace: wirhost
// ============================================================

using System;
using System.IO;
using System.Diagnostics;

Console.Title = "WirPE v13.0 - WirawOS Installer Launcher";
Console.BackgroundColor = ConsoleColor.DarkBlue;
Console.ForegroundColor = ConsoleColor.White;
Console.Clear();

Console.WriteLine("========================================");
Console.WriteLine("  WirawOS Preinstallation Environment");
Console.WriteLine("  Version 13.0");
Console.WriteLine("========================================");
Console.WriteLine();

// Запускаем установщик
RunInstaller();

Console.WriteLine();
Console.WriteLine("Press any key to exit...");
Console.ReadKey();

void RunInstaller()
{
    Console.ForegroundColor = ConsoleColor.Cyan;
    Console.WriteLine("Loading WirawOS Installer...");
    Console.ForegroundColor = ConsoleColor.White;

    string setupPath = @"D:\processes\wirinstall\setup.exe";

    if (File.Exists(setupPath))
    {
        try
        {
            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine("Starting installer: " + setupPath);
            Console.ForegroundColor = ConsoleColor.White;

            Process.Start(setupPath);

            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine("Installer started successfully!");
            Console.ForegroundColor = ConsoleColor.White;
        }
        catch (Exception ex)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine("ERROR: " + ex.Message);
            Console.ForegroundColor = ConsoleColor.White;
        }
    }
    else
    {
        Console.ForegroundColor = ConsoleColor.Red;
        Console.WriteLine("ERROR: setup.exe not found!");
        Console.WriteLine("Expected: " + Path.GetFullPath(setupPath));
        Console.ForegroundColor = ConsoleColor.White;
    }
}