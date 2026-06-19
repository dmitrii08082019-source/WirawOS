// ============================================================
// VGA.DLL - Драйвер для WirawOS на C# Native AOT
// namespace: VGA
// ============================================================

using System;
using System.Runtime.InteropServices;

namespace VGA
{
    // ============================================================
    // КОНСТАНТЫ
    // ============================================================
    public static class Constants
    {
        public const uint VGA_TEXT_MEMORY = 0xB8000;
        public const int VGA_TEXT_WIDTH = 80;
        public const int VGA_TEXT_HEIGHT = 25;

        public const byte COLOR_BLACK = 0;
        public const byte COLOR_WHITE = 15;
        public const byte COLOR_GREEN = 2;
        public const byte COLOR_RED = 4;
        public const byte COLOR_YELLOW = 14;
        public const byte COLOR_CYAN = 3;
        public const byte COLOR_MAGENTA = 5;
        public const byte COLOR_BLUE = 1;
    }

    // ============================================================
    // ОСНОВНОЙ КЛАСС ДРАЙВЕРА
    // ============================================================
    public static unsafe class VgaDriver
    {
        private static ushort* _textMemory = (ushort*)Constants.VGA_TEXT_MEMORY;
        private static int _cursorX = 0;
        private static int _cursorY = 0;

        // ============================================================
        // ИНИЦИАЛИЗАЦИЯ
        // ============================================================
        public static void Init()
        {
            Clear();
            PrintString("VGA Driver v2.0 (C# Native AOT)\n", Constants.COLOR_GREEN);
            PrintString("Driver ready for WirawOS\n", Constants.COLOR_YELLOW);
        }

        // ============================================================
        // ОЧИСТКА ЭКРАНА
        // ============================================================
        public static void Clear()
        {
            for (int i = 0; i < Constants.VGA_TEXT_WIDTH * Constants.VGA_TEXT_HEIGHT; i++)
            {
                _textMemory[i] = (ushort)((Constants.COLOR_BLACK << 8) | ' ');
            }
            _cursorX = 0;
            _cursorY = 0;
        }

        // ============================================================
        // ВЫВОД СИМВОЛА
        // ============================================================
        public static void PutChar(byte c, byte color, int x, int y)
        {
            int pos;
            if (x < 0 || y < 0)
            {
                pos = _cursorY * Constants.VGA_TEXT_WIDTH + _cursorX;
            }
            else
            {
                pos = y * Constants.VGA_TEXT_WIDTH + x;
            }

            if (c == '\n')
            {
                _cursorX = 0;
                _cursorY++;
                if (_cursorY >= Constants.VGA_TEXT_HEIGHT)
                {
                    Scroll(1);
                }
                return;
            }

            if (c == '\r')
            {
                _cursorX = 0;
                return;
            }

            if (c == '\t')
            {
                for (int i = 0; i < 4; i++)
                {
                    PutChar((byte)' ', color, -1, -1);
                }
                return;
            }

            _textMemory[pos] = (ushort)((color << 8) | c);
            _cursorX++;
            if (_cursorX >= Constants.VGA_TEXT_WIDTH)
            {
                _cursorX = 0;
                _cursorY++;
                if (_cursorY >= Constants.VGA_TEXT_HEIGHT)
                {
                    Scroll(1);
                }
            }
        }

        // ============================================================
        // ВЫВОД СТРОКИ (из IntPtr)
        // ============================================================
        public static void Print(IntPtr str, byte color)
        {
            byte* ptr = (byte*)str;
            while (*ptr != 0)
            {
                PutChar(*ptr, color, -1, -1);
                ptr++;
            }
        }

        // ============================================================
        // ВЫВОД СТРОКИ (из managed string)
        // ============================================================
        public static void PrintString(string str, byte color)
        {
            foreach (char c in str)
            {
                PutChar((byte)c, color, -1, -1);
            }
        }

        // ============================================================
        // ПРОКРУТКА
        // ============================================================
        public static void Scroll(int lines)
        {
            for (int i = 0; i < Constants.VGA_TEXT_HEIGHT - lines; i++)
            {
                for (int j = 0; j < Constants.VGA_TEXT_WIDTH; j++)
                {
                    _textMemory[i * Constants.VGA_TEXT_WIDTH + j] =
                        _textMemory[(i + lines) * Constants.VGA_TEXT_WIDTH + j];
                }
            }
            for (int i = Constants.VGA_TEXT_HEIGHT - lines; i < Constants.VGA_TEXT_HEIGHT; i++)
            {
                for (int j = 0; j < Constants.VGA_TEXT_WIDTH; j++)
                {
                    _textMemory[i * Constants.VGA_TEXT_WIDTH + j] =
                        (ushort)((Constants.COLOR_BLACK << 8) | ' ');
                }
            }
            _cursorY = Constants.VGA_TEXT_HEIGHT - lines;
        }

        // ============================================================
        // КУРСОР
        // ============================================================
        public static void SetCursor(int x, int y)
        {
            _cursorX = x;
            _cursorY = y;
        }

        public static void GetCursor(out int x, out int y)
        {
            x = _cursorX;
            y = _cursorY;
        }

        // ============================================================
        // ВЫВОД ЧИСЛА
        // ============================================================
        public static void PrintNumber(uint num, byte color)
        {
            if (num == 0)
            {
                PutChar((byte)'0', color, -1, -1);
                return;
            }

            char[] buf = new char[32];
            int i = 0;
            while (num > 0)
            {
                buf[i++] = (char)('0' + (num % 10));
                num /= 10;
            }
            for (int j = i - 1; j >= 0; j--)
            {
                PutChar((byte)buf[j], color, -1, -1);
            }
        }

        // ============================================================
        // ЭКСПОРТИРУЕМЫЕ ФУНКЦИИ ДЛЯ НЕУПРАВЛЯЕМОГО КОДА
        // ============================================================

        [UnmanagedCallersOnly(EntryPoint = "VGA_Init")]
        public static void VGA_Init()
        {
            Init();
        }

        [UnmanagedCallersOnly(EntryPoint = "VGA_Clear")]
        public static void VGA_Clear()
        {
            Clear();
        }

        [UnmanagedCallersOnly(EntryPoint = "VGA_PutChar")]
        public static void VGA_PutChar(byte c, byte color, int x, int y)
        {
            PutChar(c, color, x, y);
        }

        [UnmanagedCallersOnly(EntryPoint = "VGA_Print")]
        public static void VGA_Print(IntPtr str, byte color)
        {
            Print(str, color);
        }

        [UnmanagedCallersOnly(EntryPoint = "VGA_Scroll")]
        public static void VGA_Scroll(int lines)
        {
            Scroll(lines);
        }

        [UnmanagedCallersOnly(EntryPoint = "VGA_SetCursor")]
        public static void VGA_SetCursor(int x, int y)
        {
            SetCursor(x, y);
        }

        [UnmanagedCallersOnly(EntryPoint = "VGA_GetCursor")]
        public static void VGA_GetCursor(int* x, int* y)
        {
            *x = _cursorX;
            *y = _cursorY;
        }

        [UnmanagedCallersOnly(EntryPoint = "VGA_PrintNumber")]
        public static void VGA_PrintNumber(uint num, byte color)
        {
            PrintNumber(num, color);
        }

        [UnmanagedCallersOnly(EntryPoint = "DriverMain")]
        public static int DriverMain()
        {
            Init();
            return 0;
        }
    }
}