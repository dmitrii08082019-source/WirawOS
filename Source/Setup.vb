Imports System
Imports System.IO
Imports System.IO.Compression
Imports System.Diagnostics
Imports System.Runtime.InteropServices

Module Program

    Private Const VERSION As String = "WirawOS v13.0 Installer"
    Private Const SOURCES_ZIP As String = "sources\wirawos.zip"
    Private Const TARGET_DIR As String = "\WirawOS"

    <DllImport("shell32.dll")>
    Function SHFormatDrive(ByVal hwnd As IntPtr, ByVal drive As UInteger, ByVal fmtID As UInteger, ByVal options As UInteger) As Integer
    End Function

    <DllImport("kernel32.dll")>
    Function GetConsoleWindow() As IntPtr
    End Function

    Sub Main(ByVal args As String())
        Console.Title = VERSION
        Console.ForegroundColor = ConsoleColor.Cyan
        Console.Clear()

        Console.WriteLine("========================================")
        Console.WriteLine("  WirawOS Installer v13.0")
        Console.WriteLine("========================================")
        Console.WriteLine()

        ' Проверка прав администратора
        If Not IsAdministrator() Then
            Console.ForegroundColor = ConsoleColor.Red
            Console.WriteLine("ОШИБКА: Требуются права администратора!")
            Console.ForegroundColor = ConsoleColor.White
            Console.WriteLine("Запустите программу от имени администратора.")
            Console.WriteLine()
            Console.WriteLine("Нажмите любую клавишу для выхода...")
            Console.ReadKey()
            Return
        End If

        ' Проверка наличия источника
        If Not File.Exists(SOURCES_ZIP) Then
            Console.ForegroundColor = ConsoleColor.Red
            Console.WriteLine("ОШИБКА: " & SOURCES_ZIP & " не найден!")
            Console.ForegroundColor = ConsoleColor.White
            Console.WriteLine("Поместите wirawos.zip в папку 'sources'.")
            Console.WriteLine()
            Console.WriteLine("Нажмите любую клавишу для выхода...")
            Console.ReadKey()
            Return
        End If

        ' Получение списка дисков
        Dim drives As DriveInfo() = DriveInfo.GetDrives()
        Dim diskList As New List(Of String)()

        Console.ForegroundColor = ConsoleColor.Yellow
        Console.WriteLine("Доступные диски:")
        Console.ForegroundColor = ConsoleColor.White

        For i As Integer = 0 To drives.Length - 1
            If drives(i).IsReady AndAlso drives(i).DriveType = DriveType.Fixed Then
                diskList.Add(drives(i).Name)
                Console.WriteLine("  " & (i + 1).ToString() & ". " & drives(i).Name & " (" & FormatSize(drives(i).TotalSize) & ")")
            End If
        Next

        Console.WriteLine()

        ' Выбор диска
        Console.Write("Выберите диск (1-" & diskList.Count.ToString() & "): ")
        Dim input As String = Console.ReadLine()
        Dim selectedIndex As Integer

        If Not Integer.TryParse(input, selectedIndex) OrElse selectedIndex < 1 OrElse selectedIndex > diskList.Count Then
            Console.ForegroundColor = ConsoleColor.Red
            Console.WriteLine("Неверный выбор!")
            Console.ForegroundColor = ConsoleColor.White
            Console.WriteLine("Нажмите любую клавишу для выхода...")
            Console.ReadKey()
            Return
        End If

        Dim targetDrive As String = diskList(selectedIndex - 1)
        Dim installPath As String = targetDrive & TARGET_DIR

        Console.WriteLine()
        Console.ForegroundColor = ConsoleColor.Cyan
        Console.WriteLine("Выбран: " & targetDrive)
        Console.ForegroundColor = ConsoleColor.White

        ' ПОДТВЕРЖДЕНИЕ ФОРМАТИРОВАНИЯ
        Console.WriteLine()
        Console.ForegroundColor = ConsoleColor.Red
        Console.WriteLine("ВНИМАНИЕ: Диск " & targetDrive & " БУДЕТ ОТФОРМАТИРОВАН!")
        Console.WriteLine("ВСЕ ДАННЫЕ БУДУТ УТЕРЯНЫ!")
        Console.ForegroundColor = ConsoleColor.Yellow
        Console.Write("Продолжить? (Y/N): ")
        Dim confirm As String = Console.ReadLine()

        If confirm.ToUpper() <> "Y" Then
            Console.WriteLine()
            Console.WriteLine("Установка отменена.")
            Console.WriteLine("Нажмите любую клавишу для выхода...")
            Console.ReadKey()
            Return
        End If

        ' ФОРМАТИРОВАНИЕ ДИСКА
        Console.WriteLine()
        Console.ForegroundColor = ConsoleColor.Cyan
        Console.WriteLine("Форматирование диска " & targetDrive & "...")
        Console.ForegroundColor = ConsoleColor.White

        If Not FormatDrive(targetDrive) Then
            Console.ForegroundColor = ConsoleColor.Red
            Console.WriteLine("ОШИБКА: Не удалось отформатировать диск!")
            Console.ForegroundColor = ConsoleColor.White
            Console.WriteLine("Нажмите любую клавишу для выхода...")
            Console.ReadKey()
            Return
        End If

        Console.ForegroundColor = ConsoleColor.Green
        Console.WriteLine("OK: Диск отформатирован успешно!")
        Console.ForegroundColor = ConsoleColor.White

        ' ИЗВЛЕЧЕНИЕ ФАЙЛОВ
        Console.WriteLine()
        Console.ForegroundColor = ConsoleColor.Cyan
        Console.WriteLine("Извлечение файлов в " & installPath & "...")
        Console.ForegroundColor = ConsoleColor.White

        Try
            Directory.CreateDirectory(installPath)

            Using archive As ZipArchive = ZipFile.OpenRead(SOURCES_ZIP)
                Dim totalFiles As Integer = archive.Entries.Count
                Dim currentFile As Integer = 0

                For Each entry As ZipArchiveEntry In archive.Entries
                    currentFile += 1
                    Dim destPath As String = Path.Combine(installPath, entry.FullName)
                    Dim destDir As String = Path.GetDirectoryName(destPath)

                    If Not String.IsNullOrEmpty(destDir) Then
                        Directory.CreateDirectory(destDir)
                    End If

                    If Not String.IsNullOrEmpty(entry.Name) Then
                        Console.Write("  [" & currentFile.ToString().PadLeft(3) & "/" & totalFiles.ToString() & "] " & entry.Name.PadRight(20) & " ")
                        entry.ExtractToFile(destPath, True)
                        Console.ForegroundColor = ConsoleColor.Green
                        Console.WriteLine("OK")
                        Console.ForegroundColor = ConsoleColor.White
                    End If
                Next
            End Using

            Console.ForegroundColor = ConsoleColor.Green
            Console.WriteLine()
            Console.WriteLine("OK: Файлы извлечены успешно!")
            Console.ForegroundColor = ConsoleColor.White

        Catch ex As Exception
            Console.ForegroundColor = ConsoleColor.Red
            Console.WriteLine("ОШИБКА: " & ex.Message)
            Console.ForegroundColor = ConsoleColor.White
            Console.WriteLine("Нажмите любую клавишу для выхода...")
            Console.ReadKey()
            Return
        End Try

        ' УСТАНОВКА ЗАГРУЗЧИКА
        Console.WriteLine()
        Console.ForegroundColor = ConsoleColor.Cyan
        Console.WriteLine("Установка загрузчика...")
        Console.ForegroundColor = ConsoleColor.White

        If InstallBootloader(targetDrive, installPath) Then
            Console.ForegroundColor = ConsoleColor.Green
            Console.WriteLine("OK: Загрузчик установлен!")
            Console.ForegroundColor = ConsoleColor.White
        Else
            Console.ForegroundColor = ConsoleColor.Red
            Console.WriteLine("ПРЕДУПРЕЖДЕНИЕ: Не удалось установить загрузчик!")
            Console.ForegroundColor = ConsoleColor.White
        End If

        ' ЗАВЕРШЕНИЕ
        Console.WriteLine()
        Console.ForegroundColor = ConsoleColor.Cyan
        Console.WriteLine("========================================")
        Console.ForegroundColor = ConsoleColor.Green
        Console.WriteLine("  УСТАНОВКА ЗАВЕРШЕНА!")
        Console.ForegroundColor = ConsoleColor.White
        Console.WriteLine("  WirawOS установлен в: " & installPath)
        Console.WriteLine()
        Console.ForegroundColor = ConsoleColor.Yellow
        Console.WriteLine("  Перезагрузите компьютер и загрузитесь с диска " & targetDrive)
        Console.ForegroundColor = ConsoleColor.White
        Console.WriteLine("========================================")
        Console.WriteLine()

        Console.WriteLine("Нажмите любую клавишу для выхода...")
        Console.ReadKey()
    End Sub

    Function IsAdministrator() As Boolean
        Try
            Dim identity As Security.Principal.WindowsIdentity = Security.Principal.WindowsIdentity.GetCurrent()
            Dim principal As Security.Principal.WindowsPrincipal = New Security.Principal.WindowsPrincipal(identity)
            Return principal.IsInRole(Security.Principal.WindowsBuiltInRole.Administrator)
        Catch
            Return False
        End Try
    End Function

    Function FormatDrive(ByVal drive As String) As Boolean
        Try
            ' Пробуем через SHFormatDrive
            Dim result As Integer = SHFormatDrive(GetConsoleWindow(), GetDriveIndex(drive), 0, 0)
            If result = 1 Then Return True

            ' Если не получилось - через format.com
            Dim process As New Process()
            process.StartInfo.FileName = "format.com"
            process.StartInfo.Arguments = drive & " /Q /Y /FS:FAT32 /V:WIRAWOS"
            process.StartInfo.WindowStyle = ProcessWindowStyle.Hidden
            process.StartInfo.UseShellExecute = False
            process.StartInfo.RedirectStandardOutput = True
            process.Start()
            process.WaitForExit()
            Return process.ExitCode = 0

        Catch ex As Exception
            Return False
        End Try
    End Function

    Function GetDriveIndex(ByVal drive As String) As UInteger
        Dim letter As Char = drive.ToUpper()(0)
        Return CUInt(Asc(letter) - Asc("A"c) + 1)
    End Function

    Function FormatSize(ByVal bytes As Int64) As String
        Dim sizes As String() = {"B", "KB", "MB", "GB", "TB"}
        Dim i As Integer = 0
        Dim dbl As Double = CDbl(bytes)

        While dbl >= 1024 AndAlso i < sizes.Length - 1
            dbl /= 1024
            i += 1
        End While

        Return dbl.ToString("F2") & " " & sizes(i)
    End Function

    Function InstallBootloader(ByVal drive As String, ByVal installPath As String) As Boolean
        Try
            Dim bootBin As String = Path.Combine(installPath, "boot.bin")

            If Not File.Exists(bootBin) Then
                Return False
            End If

            Dim bootData As Byte() = File.ReadAllBytes(bootBin)
            Dim driveLetter As String = drive.Replace(":\", "").Replace(":", "")
            Dim drivePath As String = "\\.\" & driveLetter & ":"

            Using fs As New FileStream(drivePath, FileMode.Open, FileAccess.Write, FileShare.Write)
                fs.Write(bootData, 0, Math.Min(bootData.Length, 512))
                fs.Flush()
            End Using

            Return True
        Catch ex As Exception
            Return False
        End Try
    End Function

End Module